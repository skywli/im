#include "tcp_server.h"
#include "log_util.h"
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "connection.h"
#include <netutil.h>
extern int total_recv_pkt;
int  TcpServer::getConnectNum(){
    return m_conns.size();
}
void TcpServer::accept_cb(int fd, short mask, void* privdata) {
	TcpServer* pInstance = static_cast<TcpServer*>(privdata);
	if (pInstance) {
		pInstance->Accept(fd);
		pInstance->OnConn(fd);
	}
}


void TcpServer::write_cb(int fd, short mask, void* privdata)
{
	Connection* conn = static_cast<Connection*>(privdata);
	if (!conn) {
		return;
	}
	int ret = conn->write();
	if (IO_ERROR == ret) {
	
		LOGE("fd=%d send error", conn->fd);

	}
	else if (IO_CLOSED == ret) {
		
		LOGE("fd=%d close", conn->fd);
	}
	else {
		if (conn->empty()) {
            TcpServer* pInstance=static_cast<TcpServer*>(conn->pInstance);
            if(pInstance){
                pInstance->delEvent(conn->fd,SD_WRITABLE);
            }
		}
		return;
	}

	// write error
	/*aeDeleteFileEvent(loop,conn->fd, AE_READABLE | AE_WRITABLE);
	close(conn->fd);
    m_conns.remove(conn->fd);
	delete conn;*/
    TcpServer* pInstance=static_cast<TcpServer*>(conn->pInstance);
    if(pInstance){
        pInstance->OnDisconn(conn->fd);
    }
}

void TcpServer:: delEvent(int fd,int mask){
     std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
	loop->deleteFileEvent( fd, mask);

}
void TcpServer::addEvent(int fd, int mask)
{

}
void TcpServer::close_cb(int _sockfd) {
    /*
     * close the socket, we are done with it
     * poll_event_remove(poll_event, sockfd);
     */
    recv_buffers.erase(_sockfd);
    OnDisconn(_sockfd);
}


void TcpServer::parse(Connection* conn) {
	int fd = conn->fd;
	char* data = conn->buf;
	while (conn->buf_len >= HEAD_LEN) {
		//没有不完整的包
		if (!conn->less_pkt_len) {
			int* startflag=(int*)(data);
			if (ntohl(*(startflag)) == PDUBase::startflag)//正常情况下 先接收到包头
			{
				if (conn->buf_len >= HEAD_LEN) {
					int data_len = ntohl(*(int*)(data + 53));
					int pkt_len = data_len + HEAD_LEN;
					//	msg_t msg;
					PDUBase* pdu = new PDUBase;
					
					int len = conn->buf_len >= pkt_len ? pkt_len : conn->buf_len;
					_OnPduParse(data, len, *pdu);
					if (conn->buf_len >= pkt_len) {

						conn->buf_len -= pkt_len;
						conn->recv_pkt++;
						data += pkt_len;

						//	msg_info(msg);
						OnRecv(fd, pdu);	
					}
					//not enough a pkt ;
					else {
						conn->pdu = pdu;//记录不完整pdu
						conn->pkt_len = pkt_len;
						conn->less_pkt_len = pkt_len - conn->buf_len;
						conn->buf_len = 0;
						break;
					}
				}
			}
			else {//找到包头
                LOGW("err data");
				int i;
				for (i = 0; i < conn->buf_len - 3; ++i) {
					if (ntohl(*((int*)(data+i))) == PDUBase::startflag) {
						data += i;
						conn->buf_len -= i;
						break;
					}
				}
				if (i == conn->buf_len - 3) {
					conn->clear();
					break;
				}
			}
		}
		else {
			//处理不完整pdu
			int less_pkt_len = conn->less_pkt_len;
			PDUBase* pdu = conn->pdu;
			char* pData = pdu->body.get();
			int len = conn->buf_len >= less_pkt_len ? less_pkt_len : conn->buf_len;
			memcpy(pData + (conn->pkt_len - conn->less_pkt_len-HEAD_LEN), data, len);
			if (conn->buf_len >= less_pkt_len) {
				
				data += less_pkt_len;
				conn->buf_len -= less_pkt_len;
				
				//清空记录
				conn->pdu = NULL;
				conn->pkt_len = 0;
				conn->less_pkt_len = 0;
				conn->recv_pkt++;
				//msg_info(msg);
				OnRecv(fd, pdu);
			}
			else {
				conn->less_pkt_len -= conn->buf_len;
				conn->buf_len = 0;
			}
		}
	}

	//move not enough header data to buf start
	if (conn->buf_len && data != conn->buf) {
		memmove(conn->buf, data, conn->buf_len);
	}
}

void TcpServer::read_cb(int fd, short mask, void* privdata) {
	Connection* conn = static_cast<Connection*>(privdata);
	if (!conn) {
		return;
	}
	int ret = conn->read();
	if (IO_ERROR == ret) {
	    LOGE("read err,will drop the packet");
	
	}
	else if (IO_CLOSED == ret) {
		LOGE( "client close");
	}
	else {
		TcpServer* pInstance = static_cast<TcpServer*>(conn->pInstance);
		pInstance->parse(conn);
		return;
	}
	//TODO
    TcpServer* pInstance=static_cast<TcpServer*>(conn->pInstance);
    if(pInstance){
        pInstance->OnDisconn(conn->fd);
    }
    /*aeDeleteFileEvent(loop, conn->fd, AE_READABLE);
	close(conn->fd);
    m_conns.remove(conn->fd);
	delete conn;*/
}

void TcpServer::login_cb(int fd, short mask, void * privdata)
{
	Connection* conn = static_cast<Connection*>(privdata);
	if (!conn) {
		return;
	}
	TcpServer* pInstance = static_cast<TcpServer*>(conn->pInstance);
	if (pInstance) {
		pInstance->delEvent(fd, SD_WRITABLE);
		pInstance->OnLogin(conn->fd);
	}
}

TcpServer::TcpServer() {
    listen_num = 1;
    port_ = DEFAULT_PORT;
    ip_ = "127.0.0.1";
	loop = getEventLoop();
	loop->init(150000);
}

TcpServer::~TcpServer() {
    close(listen_sockfd_);
}

void TcpServer::StartServer(std::string _ip, short _port, int size) {
    ip_ = _ip;
    port_ = _port;

    PollStart(ip_.c_str(), port_,size);
}

void TcpServer::PollStart(const char *_ip, const int _port, int size) {

	for (int i = 0; i < size; ++i) {
		int fd = createTcpClient(_ip, _port);
		if (fd == -1) {
			printf("connect fail\n");
			return;
		}
        setnonblocking(fd);
		Connection* conn = new (std::nothrow)Connection;
		if (NULL == conn) {
			LOGE("alloc memory fail");
			return;
		}
		conn->fd = fd;
		conn->pInstance = this;
		std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
		auto res = m_conns.insert(std::pair<int, Connection*>(fd, conn));
		if (!res.second) {
			LOGE("insert fail");
		}
		loop->createFileEvent(fd, SD_WRITABLE, login_cb, conn);
		loop->createFileEvent(fd, SD_READABLE, read_cb, conn);
	}
	
	loop->main();
}


void TcpServer::CloseFd(int _sockfd) {
	std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
    loop->deleteFileEvent(_sockfd, SD_READABLE | SD_WRITABLE);
    std::map<int,Connection*>::iterator it=m_conns.find(_sockfd);
    if(it!=m_conns.end()){
        delete it->second;
        m_conns.erase(it);
    }
    close(_sockfd);

    //LOGD("关闭sockfd:%d", _sockfd);
}

void TcpServer::Accept(int listener)
{
	struct sockaddr_storage ss;
#ifdef WIN32
	int slen = sizeof(ss);
#else
	socklen_t slen = sizeof(ss);
#endif
	int fd = ::accept(listener, (struct sockaddr*)&ss, &slen);
	if (fd <= 0)
	{
		LOGE("accept fail");
		return;
	}
	else
	{
		//LOGW("fd=%d connect", fd);
		setnonblocking(fd);
		{
			Connection* conn = new (std::nothrow)Connection;
			if (NULL == conn) {
				LOGE("alloc memory fail");
				return;
			}
			conn->fd=fd;
			conn->pInstance = this;
			std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
			auto res=m_conns.insert(std::pair<int, Connection*>(fd, conn));
            if(!res.second){
                LOGE("insert fail");
            }
			loop->createFileEvent( fd, SD_READABLE, read_cb, conn);

		}
	}
}

bool TcpServer::Send(int _sockfd, PDUBase &_data) {
    std::shared_ptr<char> sp_buf;

    int len = OnPduPack(_data, sp_buf);
    if (len > 0) {
        bool ret = Send(_sockfd, sp_buf.get(), len);
        if (!ret) {
            LOGW("发送出错fd:%d, errno:%d, strerror:%s", _sockfd, errno, strerror(errno));
            OnSendFailed(_data);
        }
        return ret ;
    }
    return len > 0;
}

bool TcpServer::Send(int _sockfd, const char *_buffer, int _length) {
	if (_length > 0) {
		msg_t msg;
		if (bufalloc(msg, _length)) {
			memcpy(msg.m_data, _buffer, _length);
            msg.m_len=_length;
          //  std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
			std::map<int, Connection*>::iterator it = m_conns.find(_sockfd);
			if (it != m_conns.end()) {
				it->second->push(msg);
			
				loop->createFileEvent( _sockfd, SD_WRITABLE, write_cb, it->second);
				
			
				
			}
			else {
				LOGE("not find socket");
				buffree(msg);
			}
			
		}
	}
    return true;
}

void TcpServer::setlinger(int _sockfd) {
    struct linger ling;
    ling.l_onoff = 0;
    ling.l_linger = 0;
    setsockopt(_sockfd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
}

void TcpServer::setreuse(int _sockfd) {
    int opt = 1;
    setsockopt(_sockfd ,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(opt));
}

void TcpServer::setnodelay(int _sockfd) {
    int enable = 1;
    setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));

    int keepalive = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepalive, sizeof(keepalive));
    int keepcnt = 5;
    int keepidle = 30;
    int keepintvl = 1000;

    setsockopt(_sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
    setsockopt(_sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
    setsockopt(_sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));
}

int TcpServer::anetKeepAlive(int fd, int interval)
{
	int val = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1)
	{
		LOGE("setsockopt SO_KEEPALIVE: %s", strerror(errno));
		return -1;
	}

	/* Default settings are more or less garbage, with the keepalive time
	* set to 7200 by default on Linux. Modify settings to make the feature
	* actually useful. */

	/* Send first probe after interval. */
	val = interval;
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0) {
		LOGE("setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));
		return -1;
	}

	/* Send next probes after the specified interval. Note that we set the
	* delay as interval / 3, as we send three probes before detecting
	* an error (see the next setsockopt call). */
	val = interval / 3;
	if (val == 0) val = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0) {
		LOGE("setsockopt TCP_KEEPINTVL: %s\n", strerror(errno));
		return -1;
	}

	/* Consider the socket in error state after three we send three ACK
	* probes without getting a reply. */
	val = 3;
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0) {
		LOGE("setsockopt TCP_KEEPCNT: %s\n", strerror(errno));
		return -1;
	}

	((void)interval); /* Avoid unused var warning for non Linux systems. */
	return 0;
}

void TcpServer::setnonblocking(int _sockfd) {
    int opts;
    opts = fcntl(_sockfd, F_GETFL);
    if(opts < 0) {
        perror("fcntl(sock, GETFL)");
        exit(1);
    }
    opts = opts | O_NONBLOCK;
    if(fcntl(_sockfd, F_SETFL, opts) < 0) {
        perror("fcntl(sock, SETFL, opts)");
        exit(1);
    }
}

void TcpServer::bindsocket(int _sockfd, char *_pAddr, int _port) {
    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(_pAddr);
    sock_addr.sin_port = htons(_port);

    if (bind(_sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
        perror("socket bind failed.");
        exit(-1);
    }
}

void TcpServer::listensocket(int _sockfd, int _conn_num) {
    if (listen(_sockfd, _conn_num) < 0) {
        perror("socket listen failed");
        exit(-2);
    }
}
