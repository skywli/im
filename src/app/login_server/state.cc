#include "state.h"
#include <log_util.h>
#include <cnode.h>
#include <IM.State.pb.h>
#include <IM.Basic.pb.h>
#include <config_file_reader.h>
using namespace com::proto::state;
using namespace com::proto::basic;
StateService::StateService() :m_user_state_channel("user_state")
{
	m_server = NULL;
	m_cnode = NULL;
}

int StateService::init(TcpService * server,CNode* cnode)
{
	m_server = server;
	m_cnode = cnode;
	std::string redis_ip = ConfigFileReader::getInstance()->ReadString(CONF_STATE_BROADCAST_IP);
	short redis_port = ConfigFileReader::getInstance()->ReadInt(CONF_STATE_BROADCAST_PORT);
	std::string auth = ConfigFileReader::getInstance()->ReadString(CONF_STATE_BROADCAST_AUTH);
	m_state_pub = new StatePub(redis_ip, redis_port, auth);
	return m_state_pub->init();
}

void StateService::subscribe(int sockfd)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_subscribe_mutex);
	m_subscribers.push_back(sockfd);
}

int StateService::unsubscribe(int sockfd)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_subscribe_mutex);
	auto it = m_subscribers.begin();
	while (it != m_subscribers.end()) {
		if (*it == sockfd) {
			m_subscribers.erase(it);
			return 0;
		}
		++it;
	}
	return -1;
}

int StateService::publish(User * user)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_subscribe_mutex);
	UserStateBroadcast state_broadcast;
	state_broadcast.set_node_id(user->nid_);
	state_broadcast.set_state(user->online_status_);
	state_broadcast.set_user_id(user->userid_);
	state_broadcast.set_time(time(0));
	state_broadcast.set_version(user->version);
    state_broadcast.set_sockfd(user->sockfd_);
	SPDUBase spdu;
	spdu.ResetPackBody(state_broadcast, CID_USER_STAT_PUSH_REQ);
	
	int sockfd=m_cnode->getUserNodeSock(SID_MSG, user->userid_);
	if (sockfd != -1) {
		m_server->Send(sockfd, spdu);
	}
	else {
		LOGE(" publish user[id:%s] state fail", user->userid_.c_str());
	}
	if (m_state_pub->publish(m_user_state_channel, spdu.body.get(), spdu.length) == -1) {
		LOGE("publish user[%s] state fail", user->userid_.c_str());
	}
	/*auto it = m_subscribers.begin();
	while (it != m_subscribers.end()) {
		m_server->Send(*it, spdu);
		++it;
	}*/
	return 0;
}

