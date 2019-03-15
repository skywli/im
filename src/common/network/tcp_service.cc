#include "tcp_service.h"
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
#include <sys/select.h>
#include "connection.h"
#define MAX_ACCEPTS_PER_CALL  1000

extern int total_recv_pkt;
int  TcpService::getConnectNum(){
    return m_conns.size();
}

void TcpService::accept_cb(int fd, short mask, void* privdata) {
	TcpService* pInstance = reinterpret_cast<TcpService*>(privdata);
	if (pInstance) {
		pInstance->Accept(fd);
	}
}

void TcpService::write(Connection* conn){
	int ret = conn->write();
	if (IO_ERROR == ret) {
		LOGE("fd=%d send error", conn->fd);
	}
	else {
        //we must lock before check conn whether empty , in case  when conn is empty  then add msg to conn,but here delete write event;
         std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
		if (conn->empty()) {
                delEvent(conn->fd,SD_WRITABLE);
		}
		return;
	}

	// write error
	/*aeDeleteFileEvent(loop_,conn->fd, AE_READABLE | AE_WRITABLE);
	close(conn->fd);
    m_conns.remove(conn->fd);
	delete conn;*/
		
    instance_->onEvent(conn->fd, RemoteClose);
}

void TcpService::write_cb(int fd, short mask, void* privdata)
{
	Connection* conn = reinterpret_cast<Connection*>(privdata);
	if (!conn) {
		return;
	}

    TcpService* pInstance=reinterpret_cast<TcpService*>(conn->pInstance);
    if(pInstance){
        pInstance->write(conn);
    }
}

int TcpService::connect(std::string _ip, short _port)
{
	struct sockaddr_in serveraddr;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		LOGE("create sockef fail");
		return -1;
	}

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	const char *local_addr = _ip.c_str();
	if(inet_aton(local_addr, &(serveraddr.sin_addr))==0){
        LOGE("Invaild ip:%s",local_addr);
        return -1;
    }
	serveraddr.sin_port = htons(_port);

	setnonblocking(fd);
	if (connect(fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) ==-1){
		if (errno == EINPROGRESS) {
			fd_set set;
			FD_ZERO(&set);
			FD_SET(fd, &set);
			struct timeval tm;
			tm.tv_sec = 2;
			tm.tv_usec = 0;
			if (select(fd + 1, NULL,&set, NULL, &tm) <= 0) {
				LOGE("select error: %s", strerror(errno));
				close(fd);
				return -1;
			}
			if (FD_ISSET(fd, &set)) {
				socklen_t sockerr = 0;
				socklen_t len = sizeof(sockerr);
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockerr, &len) < 0) {
                    sockerr=errno;
				}
                if(sockerr){
					LOGE("socket error : %s", strerror(sockerr));
					close(fd);
					return -1;
                }
			}
			
		}
		else {
			LOGE("connect ip(%s) port(%d) fail,%s", _ip.c_str(), _port, strerror(errno));
			close(fd);
			return -1;
		}
		
	}
	Connection* conn = new (std::nothrow)Connection;
	if (NULL == conn) {
		LOGE("alloc memory fail");
		return -1;
	}
	conn->fd = fd;
	conn->pInstance = this;
	std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
	 m_conns.insert(std::pair<int, Connection*>(fd, conn));
	
	loop_->createFileEvent(fd, SD_READABLE, read_cb, conn);
    return fd;
}

long long TcpService::CreateTimer(long long milliseconds, sdTimeProc * proc, void * clientData)
{
	return loop_->createTimeEvent(milliseconds, proc, clientData);
}

void TcpService:: delEvent(int fd,int mask){
     std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
	loop_->deleteFileEvent( fd, mask);
}


void TcpService::parse(Connection* conn) {
	int fd = conn->fd;
	char* data = conn->buf;
	while (conn->buf_len >= SHEAD_LEN) {
		//没有不完整的包
		if (!conn->less_pkt_len) {
			short* startflag = (short*)(data);
			if (ntohs(*(startflag)) == SPDUBase::serverflag)//正常情况下 先接收到包头
			{
				if (conn->buf_len >= SHEAD_LEN) {
					int data_len = ntohl(*(reinterpret_cast<int*>(data + 63)));
					int pkt_len = data_len + SHEAD_LEN;
					//	msg_t msg;
					SPDUBase* pdu = new SPDUBase;

					int len = conn->buf_len >= pkt_len ? pkt_len : conn->buf_len;
					pdu->_OnPduParse(data, len);
					if (conn->buf_len >= pkt_len) {

						conn->buf_len -= pkt_len;
						conn->recv_pkt++;
						data += pkt_len;
							//	msg_info(msg);
						instance_->onData(fd, pdu);
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
				for (i = 0; i < conn->buf_len - 1; ++i) {
					if (ntohl(*((int*)(data + i))) == SPDUBase::serverflag) {
						data += i;
						conn->buf_len -= i;
						break;
					}
				}
				if (i == conn->buf_len - 1) {
					conn->clear();
					break;
				}
			}
		}
		else {
			//处理不完整pdu
			int less_pkt_len = conn->less_pkt_len;
			SPDUBase* pdu = dynamic_cast<SPDUBase*>(conn->pdu);
			char* pData = pdu->body.get();
			int len = conn->buf_len >= less_pkt_len ? less_pkt_len : conn->buf_len;
			memcpy(pData + (conn->pkt_len - conn->less_pkt_len - SHEAD_LEN), data, len);
			if (conn->buf_len >= less_pkt_len) {

				data += less_pkt_len;
				conn->buf_len -= less_pkt_len;

				//清空记录
				conn->pdu = NULL;
				conn->pkt_len = 0;
				conn->less_pkt_len = 0;
				conn->recv_pkt++;
				//msg_info(msg);
				instance_->onData(fd, pdu);
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

void TcpService::read_cb(int fd, short mask, void* privdata) {
	Connection* conn = reinterpret_cast<Connection*>(privdata);
	if (!conn) {
		return;
	}
	int ret = conn->read();
	if (IO_ERROR == ret) {
	    LOGE("read err(%s),will drop the packet",strerror(errno));
	
	}
	else if (IO_CLOSED == ret) {
		LOGE( "client close fd(%d)",fd);
	}
	else {
		TcpService* pInstance = reinterpret_cast<TcpService*>(conn->pInstance);
		pInstance->parse(conn);
		return;
	}
	//TODO
    TcpService* pInstance=reinterpret_cast<TcpService*>(conn->pInstance);
    if(pInstance){
        pInstance->instance_->onEvent(conn->fd,Disconnected);
        pInstance->closeSocket(conn->fd);
    }
    else{
        LOGE("tcpservice is null");
    }

    /*aeDeleteFileEvent(loop_, conn->fd, AE_READABLE);
	close(conn->fd);
    m_conns.remove(conn->fd);
	delete conn;*/
}

TcpService::TcpService(Instance* instance, SdEventLoop * loop):instance_(instance) {
    listen_num = 1;
	loop_ = loop;
}

TcpService::~TcpService() {
    close(listen_sockfd_);
	auto it = m_conns.begin();
	for (it; it != m_conns.end(); it++) {
		delete it->second;
	}
}

int TcpService::listen(std::string _ip, short _port) {
  
	struct sockaddr_in serveraddr;

    listen_sockfd_ = socket(AF_INET, SOCK_STREAM, 0);

    setreuse(listen_sockfd_);

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    const char *local_addr =  _ip.c_str();
    inet_aton(local_addr, &(serveraddr.sin_addr));
    serveraddr.sin_port = htons(_port);

	if (bindsocket(listen_sockfd_, local_addr, _port) == -1) {
		return -1;
	}
	if (listensocket(listen_sockfd_, SOMAXCONN) == -1) {
		return -1;
	}
	if (setnonblocking(listen_sockfd_) == -1) {
		return -1;
	}
	loop_->createFileEvent(listen_sockfd_, SD_READABLE, accept_cb, this);
}

void TcpService::run() {

    loop_->main();
	LOGD("server stop...");
}

void TcpService::stop()
{
	loop_->stop();
}

void TcpService::closeSocket(int _sockfd) {
	std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
    loop_->deleteFileEvent(_sockfd, SD_READABLE | SD_WRITABLE);
    std::map<int,Connection*>::iterator it=m_conns.find(_sockfd);
    if(it!=m_conns.end()){
        delete it->second;
        m_conns.erase(it);
    }
    close(_sockfd);

    //LOGD("关闭sockfd:%d", _sockfd);
}

void TcpService::Accept(int listener)
{
	struct sockaddr_in ss;
#ifdef WIN32
	int slen = sizeof(ss);
#else
	socklen_t slen = sizeof(ss);
#endif
    int fd;
    int max=MAX_ACCEPTS_PER_CALL;
    while(max--){
	    fd = ::accept(listener, (struct sockaddr*)&ss, &slen);
	    if (fd <= 0)
    	{
            if(errno==EINTR){
                continue;
            }
            else if(errno!=EAGAIN){

	    	    LOGE("accept fail:%s",strerror(errno));
            }
            return;
	    }
		
        LOGD("accept new connection [fd:%d,addr:%s:%d]",fd,inet_ntoa(ss.sin_addr),ntohs(ss.sin_port));
		setnonblocking(fd);
		anetKeepAlive(fd,10*60);
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
			loop_->createFileEvent( fd, SD_READABLE, read_cb, conn);

		}
		instance_->onEvent(fd, Connected);
    }
	
}

int TcpService::Send(int _sockfd, PDUBase &_data) {
    std::shared_ptr<char> sp_buf;

	msg_t msg;
    int len = OnPduPack(_data, msg.m_data);
    if (len > 0) {
		msg.m_alloc = len;
		msg.m_len = len;
       return Send(_sockfd, msg, len);
    }
    return -1;
}

int TcpService::Send(int _sockfd, const char *_buffer, int _length) {
	if (_length > 0) {
		msg_t msg;
		if (bufalloc(msg, _length)) {
			memcpy(msg.m_data, _buffer, _length);
            msg.m_len=_length;
            std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
			std::map<int, Connection*>::iterator it = m_conns.find(_sockfd);
			if (it != m_conns.end()) {
				it->second->push(msg);
				loop_->createFileEvent( _sockfd, SD_WRITABLE, write_cb, it->second);
			}
			else {
				LOGE("not find socket");
				buffree(msg);
			}
			
		}
	}
    return 0;
}

int TcpService::Send(int _sockfd,  msg_t _msg, int _length) {
	if (_length > 0) {

		std::lock_guard<std::recursive_mutex> lock_1(m_ev_mutex);
		std::map<int, Connection*>::iterator it = m_conns.find(_sockfd);
		if (it != m_conns.end()) {
			it->second->push(_msg);
			loop_->createFileEvent(_sockfd, SD_WRITABLE, write_cb, it->second);
		}
		else {
			LOGE("not find socket:%d",_sockfd);
			buffree(_msg);
			return -1;
		}
	}
	return 0;
}

void TcpService::setlinger(int _sockfd) {
    struct linger ling;
    ling.l_onoff = 0;
    ling.l_linger = 0;
    setsockopt(_sockfd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
}

void TcpService::setreuse(int _sockfd) {
    int opt = 1;
    setsockopt(_sockfd ,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(opt));
}

void TcpService::setnodelay(int _sockfd) {
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

int TcpService::anetKeepAlive(int fd, int interval)
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
	return 0;
}

int TcpService::setnonblocking(int _sockfd) {
	int opts;
	opts = fcntl(_sockfd, F_GETFL);
	if (opts < 0) {
		LOGD("fcntl(sock, GETFL)");
		return -1;
	}
	opts = opts | O_NONBLOCK;
	if (fcntl(_sockfd, F_SETFL, opts) < 0) {
		LOGD("fcntl(sock, SETFL, opts)");
		return -1;
	}
	return 0;
}

int TcpService::bindsocket(int _sockfd, const char *_pAddr, int _port) {
	struct sockaddr_in sock_addr;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = inet_addr(_pAddr);
	sock_addr.sin_port = htons(_port);

	if (bind(_sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
		LOGD("bind [ip:%s,port:%d]fail", _pAddr, _port);
		return -1;
	}
	return 0;
}

int TcpService::listensocket(int _sockfd, int _conn_num) {
	if (listen(_sockfd, _conn_num) < 0) {
		return -1;
	}
	return 0;
}
