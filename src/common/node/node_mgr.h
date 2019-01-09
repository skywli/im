#ifndef _NODE_MGR_H_
#define _NODE_MGR_H_

#include <node.h>
#include <list>
#include <pdu_base.h>
//#include <service_node.h>

#define   SOCK_CONNECTED     1     
#define   SOCK_CLOSE         0
class ServiceNode;
class SdEventLoop;


//record all access node
struct NodeInstance {
	int sid;
	int nid;
};

struct NodeConnectedState {
	int sockfd;
	int state;
	int sid;
	int nid;
};

typedef int(*dispatch)(int sockfd, SPDUBase& base, void* arg);
//called when sock connect state changed
typedef void(*connectionStateEvent)(NodeConnectedState* state, void* arg);
typedef void(*configureStateEvent)(SPDUBase& base, void* arg);
class NodeMgr{
public:
	
	static NodeMgr* getInstance();
	int init(SdEventLoop* loop, dispatch cb,void* arg);

	
	void postAppMsg(int sockfd, SPDUBase& base);

/*
	bool authenticationReq(int _sockfd, SPDUBase & _base);
	bool pingReq(int _sockfd, SPDUBase & _base);*/
	
	Node* addNode(uint32_t sid, uint32_t nid);
	Node* findNode(uint32_t sid, uint32_t nid);
	int randomGetNodeSock(uint32_t sid);
	bool available(uint32_t sid, uint32_t nid);
	Node * getAccessNode(int sid, const std::string & user_id);
	int getUserNodeId(int sid, const std::string & user_id);
	int getUserNodeSock(int sid, const std::string & user_id);
	int getUserNodeIdSock(int sid, const std::string & user_id, int & sockfd);
	ServiceNode* getServiceNode(uint32_t sid);

	void addAccessNode(int sid, int nid);
	NodeInstance* getAccessNode(int sid, int nid);
	void delAccessNode(int sid, int nid);

	void setNodeState();
	static void Timer(int fd, short mask, void * privdata);
	
	
	int sendNode(int sid, int nid, SPDUBase& base);
	int sendSid(int sid, SPDUBase& base);
	int sendRandomSid(int sid,SPDUBase & base);
	int sendsock(int sid, int sockfd,SPDUBase & base);


	void stop();
public:
	bool handler(int _sockfd, SPDUBase & _base);
	void setConnectionStateCb(connectionStateEvent cb ,void* arg);
	void connectionStateCb(NodeConnectedState* state);

	void setConfigureStateCb(configureStateEvent cb, void * arg);

	void configureStateCb(SPDUBase & base);


private:
	NodeMgr();
     void cron();
	

private:
	ServiceNode**   m_serviceNode;
	int             m_num;   

	dispatch        m_dispatch;
	void*           m_privdata;
	connectionStateEvent             m_connection_state_cb;
	void*                            m_connection_state_arg;

	configureStateEvent              m_configure_state_cb;
	void*                            m_configure_state_arg;

	//record cur node access node 
	std::list<NodeInstance*>         m_access_node_list;
	SdEventLoop*                     m_loop;
};
#endif
