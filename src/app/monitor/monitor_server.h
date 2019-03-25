
#ifndef _MONITOR_SERVICE_H
#define _MONITOR_SERVICE_H
#include "common/network/tcp_service.h"
#include "common/core/instance.h"
#include <string>
#include <monitor.h>

class MonitorServer:public Instance  {
public:
	MonitorServer();
	int init();
	int start();

	virtual void onData(int _sockfd, PDUBase* _base);
	virtual void onEvent(int fd, ConnectionEvent event);
	
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
	TcpService                  tcpService_;
    SdEventLoop*               loop_;




};

#endif
