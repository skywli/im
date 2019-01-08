#ifndef _TOOLS_CNODE_H_
#define _TOOLS_CNODE_H_
#include <define.h>
#include <list>
#include <map>
#include <string>
#include <pdu_base.h>
#include <mutex>
#define CLUSTER_SLOTS                 256

class TcpService;
class  CNode {
public:

	static CNode* getInstance();
	bool handler(int sockfd, SPDUBase & base);

	bool authenticationReq(int sockfd, SPDUBase & base);

	bool pingReq(int sockfd, SPDUBase & base);
	
private:
	class Node {
	public:
		Node(int sid, int nid) {
			this->sid = sid;
			this->nid = nid;
			fd = -1;
			connected = false;
			stragry = ROUTE_HASH;
			memset(slots, 0, sizeof(slots));
			unable = false;
		}
	public:
		int fd;
		int sid;
		int nid;
		bool connected;
		int stragry;
		std::list<std::string> user_list;
		std::string slot_str;
		char slots[CLUSTER_SLOTS / 8];
		bool   unable;
	};
public:
	int init(TcpService* net);
	int configureState(SPDUBase & base);
	bool existNode(int sid, int nid);
	int setNodeConnect(int sid, int nid, int sockfd);
	int setNodeDisconnect(int sockfd);
	
	int getUserNodeId(int sid, const std::string & user_id);
	int getUserNodeSock(int sid, const std::string & user_id);
	int getUserNodeIdSock(int sid, const std::string & user_id, int & sockfd);
private:
	CNode();
	~CNode();
	int pasreRouteSlotStr(const char * str, int len, Node * node);
	Node* addNode(int sid, int nid);
	Node* findNode(int sid, int nid);
	Node* getAccessNode(int sid, const std::string & user_id);
	
	int delNode(int sid, int nid);
	int setRouteStragry(Node* node, int stragry, const std::list<std::string>& user_list);
	int recordMsgSlot();
private:

	std::map<int, std::list<Node*>>      m_services;
	std::recursive_mutex                  m_services_mutex;
	//record msg server slot
	Node* msg_slots[CLUSTER_SLOTS];
	TcpService*                          m_pNet;

};
#endif