#include "block_tcp_client.h"
#include "log_util.h"
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <deleter.h>
#define RESEND_NUM_LIMIT 3
#define BUFF_MAX 1024*100
#define BUFF_LENGTH 1024*100

BlockTcpClient::BlockTcpClient() {
    total_buffer_ = new char[BUFF_MAX];
    buf_ = new char[BUFF_LENGTH];
    sockfd_ = -1;

}

BlockTcpClient::~BlockTcpClient() {
    if (sockfd_ != -1) {
        close(sockfd_);
    }
    if (total_buffer_ != NULL) {
        delete []total_buffer_;
        total_buffer_ = NULL;
    }
    if (buf_ != NULL) {
        delete []buf_;
        buf_ = NULL;
    }
}

int BlockTcpClient::Connect(const char *_ip, int _port, bool _is_reconnect) {
    ip_ = _ip;
    port_ = _port;
    is_reconnect_ = _is_reconnect;

    struct sockaddr_in sad;

    memset((char*)&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_port = htons((u_short)_port);
    inet_aton(_ip, &sad.sin_addr);

    if (sockfd_ > 0) {
        close(sockfd_);
        sockfd_ = -1;
    }

    sockfd_ = socket(PF_INET, SOCK_STREAM, 0);
    struct timeval timeout={3,0};//3s
    setsockopt(sockfd_,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
    setsockopt(sockfd_,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));

    int result = connect(sockfd_, (struct sockaddr*)&sad, sizeof(sad));
    if (result != 0) {
        LOGERROR(0, 0, "连接失败, ip:%s, port:%d, error:%d", _ip, _port, result);
        close(sockfd_);
        sockfd_ = -1;
        return result;
    }
    return 0;
}

int BlockTcpClient::Send(PDUBase &_base) {
    int length = 0;
    int totallen = 0;
    int resendnum = 0; // 发送失败当前重发次数

    //build package.
    std::shared_ptr<char> sp_buf;
    length = OnPduPack(_base, sp_buf);
    if (length <= 0) {
        return -3;
    }

    //若缓冲区满引起发送不完全时，需要循环发送直至数据完整
    while (totallen < length) {
        //   std::lock_guard<std::recursive_mutex> lock_1(send_mutex_);
        int write_len = write(sockfd_, sp_buf.get() + totallen, length - totallen);
        if (write_len <= 0) {
            //...重发
            resendnum++;
            if (resendnum >= RESEND_NUM_LIMIT) {
                //超过重发次数限制后，返回错误
                return -2;
            }
            /*if (is_reconnect_) {
              if (sockfd_ != -1) {
              close(sockfd_);
              sockfd_ = -1;
              }
              Connect(ip_.c_str(), port_, is_reconnect_);
              }
              usleep(2000);*/
            Connect(ip_.c_str(), port_, is_reconnect_);
            continue;
        }
        totallen += write_len;
    }
    //LOGD("TCP Send Data Out.");
    return 0;
}

int BlockTcpClient::Read(PDUBase &_base) {
    int total_length = 0;
    int len = 0;
    int readed_size = 0;

    while (true) {
        if (total_buffer_ == NULL || buf_ == NULL) {
            LOGE("total_buffer_ == NULL || buf_ == NULL");
            break;
        }
        memset(buf_, 0, BUFF_LENGTH);
        len = read(sockfd_, buf_, BUFF_LENGTH);

        if (len <= 0) {
            if (len < 0) {
                LOGE("BlockTcpClient strerror:%s", strerror(errno));
            } else {
                LOGE("BlockTcpClient disconnect.");
            }
            /*
             * closed
             * finish reading block..
             */
            close(sockfd_);
            sockfd_ = -1;
            return -1 ;
        }

        if (total_length + len > BUFF_MAX) {
            total_length = 0;
            LOGD("data too long");
            return -1;
        }
        memcpy(total_buffer_ + total_length, buf_, len);
        total_length += len;
        while((readed_size = OnPduParse(total_buffer_, total_length, _base)) > 0) {
            //remove readed data.
            total_length -= readed_size;
            if(total_length){
                memmove(total_buffer_, total_buffer_ + readed_size, total_length);
            }
            return 0;
        }
        //   usleep(100);
    }
    return -1;
}

int BlockTcpClient::SendProto(google::protobuf::Message &_msg, int _command_id, int _seq_id) {
    SPDUBase base;
    std::shared_ptr<char> body(new char[_msg.ByteSize()], carray_deleter);

    _msg.SerializeToArray(body.get(), _msg.ByteSize());
    base.body = body;
    base.length = _msg.ByteSize();
    base.command_id = _command_id;
    base.seq_id = _seq_id;

    return Send(base);
}
