#pragma once
#include <service_node.h>
#include <tcp_service.h>
class MonitorNode : public ServiceNode {
public:
	int pasreRouteSlotStr(const char * str, int len, Node * node);
	void handler(int _sockfd, SPDUBase & _base);
	MonitorNode();
	int init();
	int regist(Node * node);
	
	int registRsp(int _sockfd, SPDUBase & _base);
	bool srvinfoBroadcast(int _sockfd, SPDUBase & _base);
	
	void checkState();
	void set_id(uint32_t nid);
	
private:
	int                                  m_cur_nid;         //cur server node id;
    int                                  m_cur_sid;
	std::string                          m_monitorIP;
	uint16_t                             m_monitorPort;
	//TcpService*                     m_server;
	
    


};
