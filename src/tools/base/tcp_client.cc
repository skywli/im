#include "tcp_client.h"
#include "log_util.h"
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <deleter.h>
#define BUFF_MAX 1024 * 48
#define BUFF_LENGTH 1024 * 16


TcpClient::TcpClient() {
    sockfd_ = -1;
    is_threadrun_=false;
}

TcpClient::~TcpClient() {
    if (sockfd_ != -1) {
        close(sockfd_);
    }
}

int TcpClient::Close() {
    std::lock_guard<std::recursive_mutex> lock_(sockfd_mutex_);
    if (sockfd_ != -1) {
        close(sockfd_);
        LOGD("关闭连接%d", sockfd_);
        sockfd_ = -1;
    }
    return 1;
}

int TcpClient::Connect(const char *_ip, int _port, bool _is_reconnect) {
    ip_ = _ip;
    port_ = _port;
    is_reconnect_ = _is_reconnect;

    Close();
    while (is_threadrun_ && !is_reconnect_) {
        usleep(200000); // 休眠200ms等待接收线程结束
    }

    LOGD("connect ip:%s port:%d", _ip, _port);
    struct sockaddr_in sad;
    memset((char*)&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_port = htons((u_short)_port);
    inet_aton(_ip, &sad.sin_addr);

    sockfd_ = socket(PF_INET, SOCK_STREAM, 0);

    struct timeval oldtime;
    socklen_t len = 0;
    getsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &oldtime, &len);

    int result = connect(sockfd_, (struct sockaddr*)&sad, sizeof(sad));
    if (result != 0) {
        LOGE("connect error %d", result);
        Close();
        return result;
    }
    if (!is_threadrun_) {
        std::thread run(&TcpClient::Run, this); // c11 create a thread to run reading.
        run.detach();
    }
    LOGD("连接成功");
    OnConnect(); //call callback while connected.
    return 0;
}

int TcpClient::Send(SPDUBase &_base) {
	 std::lock_guard<std::recursive_mutex> lock_(sockfd_mutex_);
    {
        if (sockfd_ < 0) {
            LOGE("Send socket error %d", sockfd_);
            return -1;
        }
    }

    // build package
    std::shared_ptr<char> sp_buf;
    int len = OnPduPack(_base, sp_buf);
    if (len < 0) {
        LOGE("Send OnPduPack error %d", len);
        return -3;
    }

    int total_len = 0;
    int resend_num = 0;

    while (total_len < len) {
        int write_len = 0;

        {
            if (sockfd_ > 0) {
                write_len = send(sockfd_, sp_buf.get() + total_len, len - total_len, MSG_NOSIGNAL);
                if (write_len <= 0) {
                    LOGE("send errno:%d", errno);
                    Close();
                }
            }
        }

        // 重发
        if (write_len <= 0 || sockfd_ == -1) {
            resend_num++;
            if (resend_num >= resend_num_limit_) {
                // 超过重发次数限制后，返回错误
                return -2;
            }
            usleep(2000);
            continue;
        }
        total_len += write_len;
    }
    return total_len;
}

int TcpClient::SendProto(google::protobuf::Message &_msg, int _command_id, int _seq_id) {
    SPDUBase pdu_base;
    std::shared_ptr<char> body(new char[_msg.ByteSize()], carray_deleter);

    _msg.SerializeToArray(body.get(), _msg.ByteSize());
    pdu_base.body = body;
    pdu_base.length = _msg.ByteSize();
    pdu_base.command_id = _command_id;
    pdu_base.seq_id = _seq_id;

    return Send(pdu_base);
}

void TcpClient::Run() {
    char buffer[BUFF_MAX]={0};
    int len = 0;  //data len
    int avail=BUFF_MAX-len;
    PDUBase* base=new SPDUBase;

    is_threadrun_ = true;
   int readed_size=0; 
    char* pos=buffer;
    while (true) {
        int res = 0;
        if (sockfd_ > 0) {
            res = recv(sockfd_, pos, avail, 0);
            if (res <= 0) {
                Close();
                OnDisconnect();
            }
        }

        if (res <= 0) {
            LOGE("Run strerror:%s", strerror(errno));
            if (is_reconnect_) {
                LOGE("断线重连%s %d", ip_.c_str(), port_);
                Connect(ip_.c_str(), port_, is_reconnect_);
                usleep(20000);
                continue;
            }
            // 使用is_threadrun_变量做了标志 否则调用一次就会多创建一个线程
            is_threadrun_ = false;
            /*
             * closed
             * finish reading block..
             */
            // 后续应考虑重连
            break;
        }
        len+=res;
    //        LOGD("recv len:%d", len);
        char* parse_pos=buffer;
        int parse_len=len;
      //  LOGI("parse_len:%d",parse_len);
        //LOGD("解析数据");
        while ((readed_size = OnPduParse(parse_pos,parse_len, *base)) > 0) {
            // remove readed data.
            	//memmove(total_buffer, total_buffer + readed_size, total_length - readed_size);
            parse_pos+=readed_size;
            parse_len-=readed_size;
      //      LOGI("recv len:%d",base->length);
            OnRecv(base);
			base = new SPDUBase;
        }
        if(parse_len>0){
            memmove(buffer,parse_pos,parse_len);
        }
        //调整读取点
        pos=buffer+parse_len;
        len=parse_len;
        avail=BUFF_MAX-len;
        if (len >= BUFF_MAX) {
            
            len = 0;
            pos=buffer;
            avail=BUFF_MAX-len;
            continue;
        }
       // usleep(1);
    }
}
