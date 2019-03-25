#pragma once
#include "common/proto/pdu_base.h"
#include "common/network/tcp_service.h"
#include <list>
#define CLUSTER_SLOTS                 256

typedef std::list<std::string>  router_user_list_t;
class Node {
public:
	Node();
	int    route_stragry;
	router_user_list_t route_users;
	char slots[CLUSTER_SLOTS / 8] ; //store 0~255
	std::string slot_str;
	bool   enable;
	int    sid;
	int    nid;
	int    state;
	char   ip[42];
	short  port;
	int    fd;
	char   name[48];
	time_t ping;
	std::list<Node> connects;
	
};
class Monitor  {
public:
	Monitor();
	void handler(int _sockfd, SPDUBase& _base);
	int init(TcpService* server);
	static void Timer(int fd, short mask, void * privdata);
	void checkState();
	const std::map<int,std::list<Node*> >& getServices();
	int registReq(int _sockfd, SPDUBase& _base);
	bool pingReq(int _sockfd, SPDUBase & _base);
	
	Node * findNode(const char*  ip, short port, int sid);
	Node* findNode(int sid, int nid);
	int disconnectNode(int sockfd);
	void nodeInfo(Node* node,const char* tip);

	//cmd

	int addNode(const char* ip, short port, int sid);
	int stopNode(int sid, int nid);
	int delNode(int sid, int nid);
	int routeSet(int sid, int nid, int route_stragry, router_user_list_t& user_list);
	int routeSlotSet(int sid, int nid,  const char* str);

	int XMLAddNode(const char * ip, short port, int sid, int nid, const char * name, int route_stragry, router_user_list_t& user_list);
	int XMLAddNode(const char * ip, short port, int sid, int nid, const char * name, int route_stragry);
	int XMLDelNode(int sid, int nid);
	int XMLRouteNode(int sid, int nid, int route_stragry, router_user_list_t& user_list);
	int XMLRouteNode(int sid, int nid, int route_stragry, const char* slots);

	//xml

	int XMLAddNode(const char * ip, short port, int sid, int nid, const char * name);
private:
	/**
	 * flag: if flag>0,send specifity sockfd,else send all node
	*/
	int srvinfoBroadcast(int flag=0);
	Node * addNode(const char * ip, short port, int sid, int nid, const char * name);

	int pasreRouteSlotStr(const char * str, int len, Node * node);

private:
	int                           m_sid;
    int                           m_nid;
	TcpService*                    m_server;
	int                           m_cur_nid;
	int                           m_epoch;//注册次数

	std::map<int,std::list<Node*>>           m_services;
	
};
