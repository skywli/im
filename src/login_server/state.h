#pragma once
#include <time.h>
#include <user_state.h>
#include <list>
#include <tcp_service.h>
#include <pubsub.h>
using namespace State;
class CNode;
class StateService {
public:
	StateService();
	int init(TcpService* server, CNode* cnode);
	void subscribe(int sockfd);
	int unsubscribe(int sockfd);
	int publish(User* user);
	
private:
	std::list<int>   m_subscribers;
	TcpService*      m_server;

	std::recursive_mutex       m_subscribe_mutex;
	CNode*                     m_cnode;

	StatePub*                  m_state_pub;

	std::string                m_user_state_channel;
};
