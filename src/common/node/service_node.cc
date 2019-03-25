
#include "service_node.h"
#include <node_mgr.h>
#include <config_file_reader.h>
#include <stdlib.h>
#include <IM.Server.pb.h>
#include <IM.Basic.pb.h>
#include<log_util.h>
#include <monitor_node.h>
#include <define.h>
#include <hash.h>
using namespace com::proto::server;
using namespace com::proto::basic;
int g_num=0;

#define AUTH_RRT_TIME      4

#define PING_PERIORD       10
#define PING_RRT_TIME      2*(PING_PERIORD)

struct NState {
	int sid;
	int nid;
	int state;
};
std::list<NState> g_server_state_list;
ServiceNode * CreateServiceNode(uint32_t sid)
{
    if(sid==SID_MONITOR){
        return new MonitorNode();
    }
	return new ServiceNode(sid);
}

ServiceNode::ServiceNode()
{
	m_epoch = 0;
	m_sid = 0;
    memset(msg_slots, 0, sizeof(msg_slots));
}

ServiceNode::ServiceNode(int sid)
{
	m_sid = sid;
	m_epoch = 0;
}

ServiceNode::~ServiceNode()
{
	delete tcpService_;
}
int ServiceNode::init(NodeMgr * manager, SdEventLoop* loop)
{
	loop_ = loop;
	m_pNodeMgr = manager;
	tcpService_=new TcpService(this,loop);
	this->init();
	return 0;
}

int ServiceNode::reinit()
{
	this->init();
	return 0;
}

Node* ServiceNode::addNode(uint32_t sid, uint32_t nid)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	if (sid != m_sid) {
		return NULL;
	}
	else {
		Node* node = new Node(sid,nid);
		if ( node) {
			m_nodes.push_back(node);
			return node;
		}
	}
	return NULL;
}

int ServiceNode::delNode(uint32_t nid)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	std::list<Node*>::iterator it = m_nodes.begin();
	while (it != m_nodes.end()) {
		if ((*it)->getNodeId() == nid) {
			if ((*it)->getState() > NODE_CONNECT) {
				tcpService_->closeSocket((*it)->getsock());
			}
			delete *it;
			m_nodes.erase(it);
			return 0;
		}
		++it;
	}
	return -1;
}

int ServiceNode::connect(const char * ip, uint16_t port, int type)
{
	return tcpService_->connect(ip, port);
}

int ServiceNode::close(int fd)
{
	tcpService_->closeSocket(fd);
}

int ServiceNode::randomGetNodeSock()
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	int size = m_nodes.size();
	int r = rand() % size+1;
	auto it = m_nodes.begin();
	while (it != m_nodes.end()) {
		if ((*it)->getState() != NODE_ABLE) {
			continue;
		}
		if (!(--r)) {
			return (*it)->getsock();
		}
	}
	return -1;
}

void ServiceNode::handler(int sockfd, SPDUBase & base)
{
	int cid = base.command_id;
	switch (cid) {
	case CID_S2S_AUTHENTICATION_RSP:
		authenticationRsp(sockfd, base);
		delete &base;
		break;
	case CID_S2S_PONG:
		pong(sockfd, base);
		delete &base;
		break;
	default://handler by application
        m_pNodeMgr->postAppMsg(sockfd,base);
		break;
	}
	return ;
}
void ServiceNode::checkState()
{
	time_t cur_time = time(0);
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	std::list<Node*>::iterator it = m_nodes.begin();
	while (it != m_nodes.end()) {
		Node* pNode = *it;
		int state = (*it)->getState();
		if (state == NODE_CONNECT) {
			int fd = connect(pNode->getIp(), pNode->getPort(), 0);
			if (fd != -1)
			{
				pNode->clear();
				pNode->setsock(fd);
				authenticationReq(pNode);
			}
			else {

				LOGW("connect node[service(%s),node(%u)] fail", sidName(m_sid), pNode->getNodeId());
			}
		}
		else if (state == NODE_AUTH) {
			if (cur_time - pNode->auth_time > AUTH_RRT_TIME) {
				authenticationReq(pNode);
			}
		}
		++it;
	}
	heartBeat();
}

void ServiceNode::heartBeat() {
	static uint32_t     cronloops = 0;
	time_t now = time(0);
    std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	std::list<Node*>::iterator it = m_nodes.begin();

	//conn check
	it = m_nodes.begin();
	while (it != m_nodes.end()) {
		Node* node = *it;
		if (node->getState() == NODE_ABLE) {//is connected
			if (node->ping_time && //we alread ping
				now - node->ping_time > PING_RRT_TIME) {//waiting for pong timeout
				LOGE("time out recv pong,close sock");
				tcpService_->closeSocket(node->getsock());
				node->setState(NODE_CONNECT);
			}
		}
		++it;
	}

	//ping 
	it = m_nodes.begin();
	while (it != m_nodes.end()) {
		Node* node = *it;
		if (node->getState() == NODE_ABLE &&
			node->ping_time == 0 &&//we already recv pong
			now - node->pong_time>PING_RRT_TIME / 2
			) {
			ping((*it));
		}
		++it;
	}
	++cronloops;
}

bool ServiceNode::authenticationReq(Node * node)
{
	com::proto::server::AuthenticationReq req;
	int cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
	int cur_nid = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
	req.set_service_type(cur_sid);
	req.set_node_id(cur_nid);
	SPDUBase spdu;
	spdu.ResetPackBody(req, CID_S2S_AUTHENTICATION_REQ);
	tcpService_->Send(node->getsock(), spdu);
	node->setState(NODE_AUTH);
	node->auth_time = time(0);
	return true;
}

bool ServiceNode::authenticationRsp(int sockfd, SPDUBase& base)
{
	com::proto::server::AuthenticationRsp rsp;
	if (!rsp.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "authenticationRsp包解析错误");
		return -1;
	}
	uint16_t sid = rsp.service_type();
	uint32_t nid = rsp.node_id();
	Node* pNode = findNode( nid);
	if (pNode) {
		pNode->setState(NODE_ABLE);
		NodeConnectedState* state = new NodeConnectedState;
		state->sockfd = sockfd;
		state->state = SOCK_CONNECTED;
		state->sid = sid;
		state->nid = nid;
		m_pNodeMgr->connectionStateCb(state);
		delete state;
		LOGD("authentication with node[sid:%s,nid:%d] success", sidName(sid), nid);
	}
	else {
		LOGE("not find node[sid:%s,nid:%d],authenticationRsp fail", sidName(sid), nid);
	}
	return true;
}

bool ServiceNode::ping(Node * node) {
	SPDUBase spdu;
	PingReq req;
	int cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
	int cur_nid = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
	req.set_service_type(cur_sid);
	req.set_node_id(cur_nid);
	req.set_epoch(m_epoch);
	if (m_sid == SID_MONITOR) {
		m_pNodeMgr->setNodeState();
		auto state_list = req.mutable_state_list();
		auto it = g_server_state_list.begin();
		while (it != g_server_state_list.end()) {
			NState& state = *it;
			auto item = state_list->Add();
			item->set_node_id(state.nid);
			item->set_service_type(state.sid);
			item->set_state(state.state);
			++it;
		}
		g_server_state_list.clear();
	}
	spdu.ResetPackBody(req, CID_S2S_PING);
	node->ping_time = time(0);
	//LOGD("ping node[sid:%s,nid:%d,fd:%d]",sidName(node->getSid()),node->getNid(),node->getsock());
	tcpService_->Send(node->getsock(), spdu);
}

bool ServiceNode::pong(int sockfd, SPDUBase& base)
{
	PongRsp rsp;
	if (!rsp.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "pongRsp包解析错误");
		return -1;
	}
	int sid = rsp.service_type();
	int nid = rsp.node_id();
    //LOGD("Pong[sid:%s,nid:%d]",sidName(sid),nid);
	Node* node = findNode(nid);
	if (node) {
		node->ping_time = 0;
		node->pong_time = time(0);
	}
	return true;
}


int ServiceNode::init()
{
	return 0;
}


bool ServiceNode::available(uint32_t nid)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	Node * node = findNode(nid);
	if (node) {
		int state = node->getState();
		if (NODE_ABLE == state || state==NODE_AUTH) {
			return true;
		}
		else {
			LOGW( "sid(%u),node(%u) unavailable", m_sid, nid);
		}
	}
	return false;
}

Node * ServiceNode::findNode(uint32_t nid)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	std::list<Node*>::iterator it = m_nodes.begin();
	while (it != m_nodes.end()) {
		if ((*it)->getNodeId() == nid) {
			return *it;
		}
		++it;
	}
	return NULL;
}

Node * ServiceNode::getNodeBySock(int sockfd)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	auto it = m_nodes.begin();
	for (it; it != m_nodes.end(); it++) {
		Node* node = *it;
		if (node->getsock() == sockfd && node->getState() != NODE_CONNECT) {
			return node;
		}
	}
	return NULL;
}

uint16_t ServiceNode::getServiceId()
{
	return m_sid;
}

void ServiceNode::addServerState(Node * node)
{
	NState state;
	state.nid = node->getNid();
	state.sid = node->getSid();
	state.state = node->getState();
	g_server_state_list.push_back(state);
}

int ServiceNode::sendNode(int nid, SPDUBase & base)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	std::list<Node*>::iterator it = m_nodes.begin();
	while (it != m_nodes.end()) {
		if ((*it)->getNodeId() == nid) {
			if ((*it)->getState() == NODE_ABLE) {
				return tcpService_->Send((*it)->getsock(), base);
			}
		}
		++it;
	}
	LOGE("not find able node[%s,%d]", sidName(m_sid), nid);
	return -1;
}

int ServiceNode::sendRandomSid(SPDUBase & base)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	auto it = m_nodes.begin();
	while (it != m_nodes.end()) {
		if ((*it)->getState() == NODE_ABLE) {
			return tcpService_->Send((*it)->getsock(), base);
		}
		++it;
	}
	LOGE("not find able node[%s]", sidName(m_sid));
	return -1;
}

int ServiceNode::sendSid(SPDUBase & base)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	auto it = m_nodes.begin();
	while (it != m_nodes.end()) {
		if ((*it)->getState() == NODE_ABLE) {
			return tcpService_->Send((*it)->getsock(), base);
		}
		++it;
	}
	LOGE("not find able node[%s]", sidName(m_sid));
	return -1;
}

int ServiceNode::sendsock(int sockfd, SPDUBase & base)
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	std::list<Node*>::iterator it = m_nodes.begin();
	while (it != m_nodes.end()) {
		if ((*it)->getsock() == sockfd) {
			if ((*it)->getState() == NODE_ABLE) {
				return tcpService_->Send(sockfd, base);
			}
		}
		++it;
	}
	LOGE("not find able node[%s]", sidName(m_sid));
	return -1;
}

void ServiceNode::setNodeState()
{
	std::lock_guard<std::recursive_mutex> lock(m_nodes_mutex);
	std::list<Node*>::iterator it = m_nodes.begin();
	while (it != m_nodes.end()) {
		Node* pNode = *it;
		addServerState(pNode);
		++it;
	}
}


void ServiceNode::onData(int sockfd, PDUBase * base)
{
	SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
	handler(sockfd, *spdu);
}

void ServiceNode::onEvent(int fd, ConnectionEvent event)
{
	if (event == Disconnected) {
		std::list<Node*>::iterator it = m_nodes.begin();
		while (it != m_nodes.end()) {
			Node* pNode = *it;
			if (fd == pNode->getsock()) {
				LOGE("node[sid:%s,node_id:%d] disconnect", sidName(pNode->getSid()), pNode->getNid());
				pNode->clear();
				pNode->setState(NODE_CONNECT);
				NodeConnectedState* state = new NodeConnectedState;
				state->sockfd = fd;
				state->state = SOCK_CLOSE;
				state->sid = pNode->getSid();
				state->nid = pNode->getNid();
				m_pNodeMgr->connectionStateCb(state);
				delete state;
				break;
			}
			++it;
		}
		tcpService_->closeSocket(fd);
	}
}

int ServiceNode::recordMsgSlot()
{
	for (auto nit = m_nodes.begin(); nit != m_nodes.end(); nit++) {
		if ((*nit)->stragry == ROUTE_SLOT) {
			for (int i = 0; i<CLUSTER_SLOTS / 8; i++) {
				for (int j = 0; j<8; j++) {
					if ((*nit)->slots[i] & 1 << j) {
						//printf("%d   ", 8 * i + j);
						msg_slots[8 * i + j] = *nit;
					}
				}
			}
		}
	}
}


Node* ServiceNode::getAccessNode( const std::string& user_id)
{
	int unable = 0;
	for (auto nit = m_nodes.begin(); nit != m_nodes.end(); ) {
		Node* node = *nit;
	
		if (node->getState() != NODE_ABLE) {
			++unable;
			nit++;
			continue;
		}
		if (node->stragry == ROUTE_USER) {
			++unable;
			for (auto uit = node->user_list.begin(); uit != node->user_list.end(); uit++) {
				if (*uit == user_id) {
					LOGD("user[uid:%s] route to node[sid:%s nid:%d] according to ROUTE_USER", user_id.c_str(), sidName(m_sid), node->getNid());
					return node;
				}
			}
		}
		nit++;
	}
	const char* p = user_id.c_str();
	if (m_sid == SID_MSG) {
		//slot
		int slot = dictGenHashFunction((const unsigned char*)p, user_id.length()) % CLUSTER_SLOTS;
		Node* node = msg_slots[slot];
		if (node) {
			//LOGD("user[uid:%s] route to node[sid:%s,nid:%d] according to ROUTE_SLOTS", user_id.c_str(), sidName(sid),node->nid);
			return node;
		}
		LOGE("user[uid:%s] route to sid:%s according to ROUTE_SLOTS  not find node", user_id.c_str(), sidName(m_sid));
		return NULL;
	}


	int able = m_nodes.size() - unable;
	if (able && able > 0) {
		int index = dictGenHashFunction((const unsigned char*)&user_id, sizeof(int)) % able;
		for (auto nit = m_nodes.begin(); nit != m_nodes.end(); nit++) {
			Node* node = *nit;
			if (node->getState() != NODE_ABLE) {
				continue;
			}
			if (!index) {
				//LOGD("user[uid:%s] route to sid:%s according to ROUTE_HASH", user_id.c_str(), sidName(sid));
				return node;
			}
			--index;
		}
	}

	LOGE("user[uid:%s] route to sid:%s  not find node", user_id.c_str(), sidName(m_sid));
	return NULL;
}
