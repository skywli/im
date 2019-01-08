#ifndef _SERVICENODE_H
#define _SERVICENODE_H

#include <node.h>
#include <tcp_service.h>
#include <list>
#include <mutex>
#include <node_mgr.h>


class ServiceNode:public TcpService {
public:
	ServiceNode();
	ServiceNode(int   sid);
	virtual ~ServiceNode();
	int init(NodeMgr * manager, SdEventLoop* loop);

    Node* addNode(uint32_t sid, uint32_t nid);
	int delNode(uint32_t nid);
	int connect(const char* ip, uint16_t port, int type);
	int disconnect(int sid, int nid);
	int close(int fd);
	int randomGetNodeSock();
	bool available(uint32_t nid);
	Node* findNode(uint32_t nid);
	Node* getNodeBySock(int sockfd);
	uint16_t  getServiceId();

	/**
	* brief: report to monitor node state
	*/
	void addServerState(Node* node);

	

	int sendNode(int nid, SPDUBase & _base);
	int sendRandomSid( SPDUBase & _base);
	virtual int sendSid(SPDUBase& base);
	int sendsock(int sockfd,SPDUBase& base);
	void setNodeState();
	virtual void checkState();

	Node* randomGetNode();
	virtual void handler(int _sockfd, SPDUBase & _base) ;

	virtual int init();

public:
	virtual void OnRecv(int _sockfd, PDUBase* _base);
	virtual void OnConn(int _sockfd);
	virtual void OnDisconn(int _sockfd);

	int recordMsgSlot();

	Node * getAccessNode(const std::string & user_id);

protected:
	void heartBeat();
private:
	int reinit();
	
	bool authenticationReq(Node* node);
	bool authenticationRsp(int sockfd, SPDUBase & base);
	bool ping(Node*);
	bool pong(int sockfd, SPDUBase & base);

protected:

	int                            m_sid;
	NodeMgr*                       m_pNodeMgr;
	std::list<Node*>               m_nodes;
	std::recursive_mutex           m_nodes_mutex;
	long long                      m_epoch;
private:
	SdEventLoop*                   m_loop;
//	TcpService*                     m_pNet;
	Node*                          msg_slots[CLUSTER_SLOTS];
	
	


	

};

ServiceNode* CreateServiceNode(uint32_t sid);
#endif
