#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include "pdu_base.h"
#include "pdu_util.h"
#include "connection.h"
#include <unordered_map>
#include <mutex>
#include <map>
#include <sdEventloop.h>
#define DEFAULT_PORT 8080
#define TCP_MAX_BUFF 51200
#define MAX_EVENTS 100
#define TIMEOUT_INTERVAL 100

#define CONFIG_FDSET_INCR  128

class ConnectBuffer {
public:
    std::shared_ptr<char> body;
    int length;
    ConnectBuffer() {
        length = 0;
    }
};

class TcpServer:public PduUtil {
public:
	
	TcpServer();
    int getConnectNum();
    virtual ~TcpServer();
    virtual void OnRecv(int _sockfd, PDUBase &_base) = 0;
	virtual void OnRecv(int _sockfd, PDUBase* _base) = 0;
    virtual void OnConn(int _sockfd) = 0;
    virtual void OnDisconn(int _sockfd) = 0;
    virtual void OnSendFailed(PDUBase &_data) = 0;
	virtual void OnLogin(int _sockfd) = 0;
    virtual void StartServer(std::string _ip, short _port ,int size);

    void delEvent(int fd,int mask);
	void addEvent(int fd, int mask);

    /**********************************************************
     * this is app level buffer for tcp.
     * if a connect named A , send buffer 100Byte to server.
     * server recv 50Byte, other 50Byte did not recv due to network question
     * mean while, another connection named B send 100Byte to server.
     * server recv 100Byte.
     */
    std::unordered_map<int, ConnectBuffer> recv_buffers;

public:
    int epollfd_;
    int listen_sockfd_;


protected:
    std::string ip_; // 服务端监听IP
    int port_; // 服务端监听端口
    int listen_num;	// 监听的SOCKET数量

    std::mutex send_mutex;

    void PollStart(const char *_ip, const int _port,int size);
    void CloseFd(int _sockfd);
	void Accept(int _sockfd);
    bool Send(int _sockfd, PDUBase &_data);
    bool Send(int _sockfd, const char *buffer, int length);
	
	
	static void write_cb(int fd, short mask, void* privdata);

	//  void accept_cb(int _sockfd, char *_ip, short _port);
    void close_cb(int _sockfd);
    int read_cb(int _sockfd, struct epoll_event &_ev);

	void parse(Connection * conn);

    void setlinger(int _sockfd);
    void setreuse(int _sockfd);
    void setnodelay(int _sockfd);
	int anetKeepAlive(int fd, int interval);
	void setnonblocking(int _sockfd);
    void bindsocket(int _sockfd, char *_pAddr, int _port);
    void listensocket(int _sockfd, int _conn_num);

private:
	static  void accept_cb(int fd, short mask,void* privdata);
	static void read_cb(int fd, short mask, void* privdata);
	static void login_cb(int fd, short mask, void* privdata);
private:
	SdEventLoop*  loop;
	std::map<int, Connection*> m_conns;
	std::recursive_mutex       m_ev_mutex;
};

#endif
