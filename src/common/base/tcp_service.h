#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include "pdu_base.h"
#include "pdu_util.h"
#include "connection.h"
#include <unordered_map>
#include <mutex>
#include <map>
#include <sd_event_loop.h>
#include<pdu_util.h>

#define TCP_MAX_BUFF 51200
#define MAX_EVENTS 100
#define TIMEOUT_INTERVAL 100

#define CONFIG_FDSET_INCR  128


#define run_with_period(_ms_) if (!(cronloops%((_ms_)/1000)))

class TcpService:public PduUtil {
public:
	
	TcpService();
    int getConnectNum();
    virtual ~TcpService();
	int init(SdEventLoop * loop);
	virtual void OnRecv(int _sockfd, PDUBase* _base) = 0;
    virtual void OnConn(int _sockfd) = 0;
    virtual void OnDisconn(int _sockfd) = 0;
  

    virtual int StartServer(std::string _ip, short _port);
	
	int StartClient(std::string _ip, short _port);
	long long  CreateTimer(long long milliseconds, sdTimeProc * proc, void * clientData);
    void delEvent(int fd,int mask);

	int Send(int _sockfd, PDUBase &_data);
	int Send(int _sockfd, const char *buffer, int length);

	int Send(int _sockfd, msg_t _msg, int _length);
	void CloseFd(int _sockfd);
	void stop();
public:
    int epollfd_;
    int listen_sockfd_;


protected:
  
    int listen_num;	// 监听的SOCKET数量

    std::mutex send_mutex;

    void PollStart();
	
	void Accept(int _sockfd);
  
    void write(Connection* conn);
	static void write_cb(int fd, short mask, void* privdata);

	//  void accept_cb(int _sockfd, char *_ip, short _port);
    void close_cb(int _sockfd);
    int read_cb(int _sockfd, struct epoll_event &_ev);

	virtual void parse(Connection * conn);

    void setlinger(int _sockfd);
    void setreuse(int _sockfd);
    void setnodelay(int _sockfd);
	int anetKeepAlive(int fd, int interval);
	int setnonblocking(int _sockfd);
	int bindsocket(int _sockfd, const char *_pAddr, int _port);
	int listensocket(int _sockfd, int _conn_num);

private:
	static  void accept_cb(int fd, short mask,void* privdata);
	static void read_cb(int fd, short mask, void* privdata);
private:
	SdEventLoop*  m_loop;
	std::map<int, Connection*> m_conns;
	std::recursive_mutex       m_ev_mutex;
};

#endif
