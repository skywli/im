
#ifndef _CONNECTION_SERVICE_H
#define _CONNECTION_SERVICE_H

#include <list>
#include <string>
#include <block_tcp_client.h>


class MonitorClient:public BlockTcpClient {
public:
	MonitorClient();
	int init();
	int start();
	int processCmd(int argc, char* argv[]);
	int read();
	int clusterState( SPDUBase& base);
	int configSetRsp( SPDUBase& base);
	
	virtual void OnRecv(PDUBase* _base);
	virtual void OnConnect();
	virtual void OnDisconnect();
	

private:
	// 本服务器的ip和端口
	std::string    m_ip;
	int            m_port;

};

#endif
