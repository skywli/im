
#ifndef _DISPATCH_SERVICE_H
#define _DISPATCH_SERVICE_H

#include <string>
#include<tcp_service.h>
#include <list>
#include <define.h>
#include <CNode.h>
class DispatchServer:public TcpService  {

	//who connect me
private:
	class Node {
	public:
		Node(int sid, int nid) {
			this->sid = sid;
			this->nid = nid;
			fd = -1;
			connected = false;
			stragry = ROUTE_HASH;
		}
	public:
		int fd;
		int sid;
		int nid;
		bool connected;
		int stragry;
		std::list<int> user_list;
		std::string slots_str;
	};

public:
	DispatchServer();
	int init();
	void reportOnliners();
	int start();

	virtual void OnRecv(int _sockfd, PDUBase* _base);
	virtual void OnConn(int _sockfd);
	virtual void OnDisconn(int _sockfd);
	static void connectionStateEvent(int sockfd, int state, int sid, int nid, void * arg);
	int getsock(int cmd,int user_id);
	int registCmdReq(int sockfd, SPDUBase& base);
	/**
	* brief:handler busi msg
	*/
	int processBusiMsg(int sockfd, SPDUBase& base);
	/**
	* brief:dispatch msg to busi server
	*/
	int dispatchMsg(int sockfd, SPDUBase& base);
	static int innerMsgCb(int sockfd, SPDUBase& base,void* arg);
	
	static void configureStateCb(SPDUBase & base, void * arg);
	int configureState(SPDUBase & base);
	Node* findNode(int sid, int nid);
	int disconnNode(int sockfd);
	DispatchServer::Node * getAccessNode(int sid, int user_id);
	
	Node* addNode(int sid, int nid);
	int delNode(int sid, int nid);
	int setRouteStragry(Node* node,int stragry,const std::list<int>& user_list);
private:
	static void Timer(int fd, short mask, void * privdata);
	


private:
	
	std::string                         m_ip;
	int                                 m_port;
	
	TcpService*                          m_server;
//	std::list<Node*>                    m_nodes;//dispatch client
	std::map<int, int>                  m_cmd_map;// cmd-sid
	SdEventLoop*                        m_loop;
	 
	std::map<int, std::list<Node*>>      m_services;
	CNode*                                m_pNode;
};

#endif
