#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include  "common/proto/pdu_util.h"
#include "connection.h"
#include <unordered_map>
#include <mutex>
#include <map>
#include "common/event/sd_event_loop.h"
#include  "common/proto/pdu_base.h"
#include "common/core/instance.h"
#define TCP_MAX_BUFF 51200
#define MAX_EVENTS 100
#define TIMEOUT_INTERVAL 100

#define CONFIG_FDSET_INCR  128


#define run_with_period(_ms_) if (!(cronloops%((_ms_)/1000)))

class TcpService:public PduUtil {
public:
	
	TcpService(Instance* instance, SdEventLoop * loop);
	~TcpService();
    int getConnectNum();
   
    int listen(std::string _ip, short _port);
	
	int connect(std::string _ip, short _port);
	long long  CreateTimer(long long milliseconds, sdTimeProc * proc, void * clientData);
    void delEvent(int fd,int mask);

	int Send(int _sockfd, PDUBase &_data);
	int Send(int _sockfd, const char *buffer, int length);

	int Send(int _sockfd, msg_t _msg, int _length);
	void closeSocket(int _sockfd);
	void stop();
	void run();
public:
    int epollfd_;
    int listen_sockfd_;


protected:
  
    int listen_num;	// 监听的SOCKET数量

    std::mutex send_mutex;

	void Accept(int _sockfd);
  
    void write(Connection* conn);
	static void write_cb(int fd, short mask, void* privdata);

	//  void accept_cb(int _sockfd, char *_ip, short _port);
    void close_cb(int _sockfd);
    int read_cb(int _sockfd, struct epoll_event &_ev);

    void parse(Connection * conn);

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
	SdEventLoop*  loop_;
	std::map<int, Connection*> m_conns;
	std::recursive_mutex       m_ev_mutex;
	Instance*        instance_;
};

#endif
