
#ifndef _MONITOR_SERVICE_H
#define _MONITOR_SERVICE_H

#include <string>
#include<tcp_service.h>
#include <monitor.h>

class MonitorServer:public TcpService  {
public:
	MonitorServer();
	int init();
	int start();

	virtual void OnRecv(int _sockfd, PDUBase* _base);
	virtual void OnConn(int _sockfd);
	virtual void OnDisconn(int _sockfd);
	
	int clusterStatus(int sockfd, SPDUBase& base);
	int configSet(int sockfd, SPDUBase& base);

	
private:
	
	static void Timer(int fd, short mask, void * privdata);

	//cmd 
	int addNode(int sockfd, std::string * cmd, int size);
	//cmd 
	int delNode(int sockfd, std::string * cmd, int size);
	int stopNode(int sockfd, std::string * cmd, int size);
	//cmd 
	int routeSet(int sockfd, std::string * cmd, int size);

	void replyInfo(int sockfd, const char * content, ...);

private:
	
	std::string               m_ip;
	int                       m_port;
	Monitor                   m_monitor;
	TcpService*                m_server;
    SdEventLoop*               m_loop;



};

#endif
