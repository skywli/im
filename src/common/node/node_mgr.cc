#include "node_mgr.h"
#include <config_file_reader.h>
#include <IM.Server.pb.h>
#include <IM.Basic.pb.h>
#include<log_util.h>
#include <stdlib.h>
#include <service_node.h>
using namespace com::proto::server;
using namespace com::proto::basic;

#define  MAX_SERVICE_TYPE        100

void NodeMgr::setConnectionStateCb(connectionStateEvent cb,void* arg)
{
	m_connection_state_cb = cb;
	m_connection_state_arg = arg;
}

void NodeMgr::connectionStateCb(NodeConnectedState* state)
{
	if (m_connection_state_cb) {
		m_connection_state_cb(state, m_connection_state_arg);
	}
}

void NodeMgr::setConfigureStateCb(configureStateEvent cb, void * arg)
{
	m_configure_state_cb = cb;
	m_configure_state_arg = arg;
}

void NodeMgr::configureStateCb(SPDUBase& base)
{
	if (m_configure_state_cb) {
		m_configure_state_cb(base, m_configure_state_arg);
	}
}

NodeMgr::NodeMgr()
{
	m_num = 0;
	m_dispatch = NULL;
	m_privdata = NULL;

	m_connection_state_cb = NULL;
	m_connection_state_arg = NULL;
	m_configure_state_cb = NULL;
	m_configure_state_arg = NULL;
}

NodeMgr * NodeMgr::getInstance()
{
	static NodeMgr instance;
	return &instance;
}

int NodeMgr::init( SdEventLoop* loop, dispatch cb, void* data)
{
	m_dispatch = cb;
	m_privdata = data;
	loop_ = loop;
	//TcpService::init(loop);
	TiXmlElement *root = ConfigFileReader::getInstance()->getRoot();
	TiXmlElement* child;
	if (!ConfigFileReader::getInstance()->getNodePointerByName(root, CONF_CONN_TYPE, child)) {
		LOGE("not find conn type");
		return -1;
	}
	int i = 0;
	std::list<const char*> service_list;
	for (TiXmlElement* elem = child->FirstChildElement(); elem != NULL; elem = elem->NextSiblingElement()) {
		TiXmlNode* e1 = elem->FirstChild();
		LOGD("conn type:%s", e1->ToText()->Value());
		service_list.push_back(e1->ToText()->Value());
	}
	m_num=service_list.size();
	m_serviceNode=(ServiceNode**)malloc(m_num*sizeof(ServiceNode*));	
	ServiceNode* snode = NULL;
	for (auto it=service_list.begin(); it!=service_list.end(); ++it) {
		snode = CreateServiceNode(getSid(*it));
		if(snode){
	            m_serviceNode[i++]=snode;
		    snode->init(this, loop_);
		}
	}
	loop_->createTimeEvent(1000, Timer, this);
	return 0;
}


void NodeMgr::postAppMsg(int sockfd, SPDUBase & base)
{
	if (m_dispatch) {
		m_dispatch(sockfd, base, m_privdata);
	}
}

#if 0
bool NodeMgr::handler(int sockfd, SPDUBase& base)
{
	ServiceNode* snode = NULL;
	int cid = base.command_id;
	switch (cid) {
	case CID_S2S_AUTHENTICATION_REQ:
		authenticationReq(sockfd, base);
		break;
	case CID_S2S_PING:
		pingReq(sockfd, base);
		break;
	default://other msg handler by application
		LOGD("unknow cmd(%d)", cid);
		break;
	}
	return false;
}
bool NodeMgr::authenticationReq(int sockfd, SPDUBase& base)
{
	com::proto::server::AuthenticationReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "authenticationReq包解析错误");
		return -1;
	}
	uint16_t sid = req.service_type();
	uint32_t nid = req.node_id();
	LOGD("recv node[sid:%s,nid:%d] authenticationReq",sidName(sid),nid);
	//only  after recving broadcast,we can consider the node able;
	if (!getAccessNode(sid, nid)) {
		return false;
	}
	/*ServiceNode* snode=getServiceNode(sid);
	if (!snode) {
		snode=CreateServiceNode(sid);
		if (!snode) {
			return false;
		}
		Node* node=snode->addNode(sid, nid);
		node->type = NODE_CLIENT;
	}*/
	int cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
	int cur_nid = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
	if (cur_nid == 0 | cur_sid == 0) {
		LOGD("node[sid:%s,nid:%d] authenticationReq fail", sidName(sid), nid);
		return false;
	}
	com::proto::server::AuthenticationRsp rsp;
	rsp.set_node_id(cur_nid);
	rsp.set_service_type(cur_sid);
	rsp.set_result_code(1);
	SPDUBase pdu;
	pdu.ResetPackBody(rsp, CID_S2S_AUTHENTICATION_RSP);
	m_pNet->Send(sockfd, pdu);
//	connectionStateCb(sockfd, SOCK_CONNECTED, sid, nid);
	LOGD("node[sid:%s,nid:%d] authenticationReq success", sidName(sid), nid);
	return false;
}

bool NodeMgr::pingReq(int sockfd, SPDUBase& base)
{
	PingReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "authenticationReq包解析错误");
		return -1;
	}
	int sid = req.service_type();
	int nid = req.node_id();
  //  LOGD("recv node[sid:%s,nid:%d] ping",sidName(sid),nid);
	PongRsp rsp;
    int cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
    int cur_nid = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
	rsp.set_service_type(cur_sid);
	rsp.set_node_id(cur_nid);
	base.ResetPackBody(rsp, CID_S2S_PONG);
	m_pNet->Send(sockfd, base);
	return false;
}

#endif
Node* NodeMgr::addNode(uint32_t sid, uint32_t nid)
{
	ServiceNode* snode;
	if (NULL == (snode=getServiceNode(sid))) {
		LOGW( "sid(%s) not regist", sidName(sid));
		return NULL;
	}
	return snode->addNode(sid, nid);
}

ServiceNode * NodeMgr::getServiceNode(uint32_t sid)
{
	for (int i = 0; i < m_num; i++) {
		if (m_serviceNode[i] && m_serviceNode[i]->getServiceId() == sid) {
			return m_serviceNode[i];
		}
	}
	//LOGD( "not regist sid(%s)", sidName(sid));
	return NULL;
}

void NodeMgr::addAccessNode(int sid, int nid)
{
	if (getAccessNode(sid, nid)) {
		return;
	}
	NodeInstance*  node = new NodeInstance;
	node->sid = sid;
	node->nid = nid;
	m_access_node_list.push_back(node);
}

NodeInstance * NodeMgr::getAccessNode(int sid, int nid)
{
	auto it = m_access_node_list.begin();
	while (it != m_access_node_list.end()) {
		if ((*it)->sid == sid && (*it)->nid == nid) {
			return *it;
		}
		it++;
	}
	return NULL;
}

void NodeMgr::delAccessNode(int sid, int nid)
{
	auto it = m_access_node_list.begin();
	while (it != m_access_node_list.end()) {
		if ((*it)->sid == sid && (*it)->nid == nid) {
			m_access_node_list.erase(it);
			break;
		}
		it++;
	}
	
}

void NodeMgr::setNodeState()
{
	for (int i = 0; i < m_num; i++) {
		if (m_serviceNode[i]) {
			m_serviceNode[i]->setNodeState();
		}
	}
}

void NodeMgr::Timer(int fd, short mask, void * privdata)
{
    NodeMgr* pNodeMgr=reinterpret_cast<NodeMgr*>(privdata);
    if(pNodeMgr){
        pNodeMgr->cron();
    }
}

int NodeMgr::sendNode(int sid, int nid, SPDUBase& base)
{
	ServiceNode* snode = getServiceNode(sid);
	if (!snode) {
		LOGE("not regist sid:%s", sidName(sid));
		return -1;
	}
	return snode->sendNode(nid, base);
}

int NodeMgr::sendSid(int sid, SPDUBase & base)
{
	ServiceNode* snode = getServiceNode(sid);
	if (!snode) {
		return -1;
	}
	return snode->sendSid(base);
}

int NodeMgr::sendRandomSid(int sid, SPDUBase & base)
{
	ServiceNode* snode = getServiceNode(sid);
	if (!snode) {
		return -1;
	}
	return snode->sendRandomSid(base);
}

int NodeMgr::sendsock(int sid, int sockfd, SPDUBase & base)
{
	ServiceNode* snode = getServiceNode(sid);
	if (!snode) {
		LOGE("not regist sid:%s", sidName(sid));
		return -1;
	}
	return snode->sendsock(sockfd, base);
}

void NodeMgr::stop()
{
	loop_->stop();
}

void NodeMgr::cron()
{
	for (int i = 0; i < m_num; i++) {
		if (m_serviceNode[i] ) {
			m_serviceNode[i]->checkState();
			//m_serviceNode[i]->heartBeat();
		}
	}
}

Node* NodeMgr::findNode(uint32_t sid, uint32_t nid)
{
	ServiceNode * snode = getServiceNode(sid);
	if (snode) {
		return snode->findNode(nid);
	}
	return NULL;
}

int NodeMgr::randomGetNodeSock(uint32_t sid)
{
	ServiceNode* snode=getServiceNode(sid);
	if (snode) {
		return snode->randomGetNodeSock();
	}
	return -1;
}

bool NodeMgr::available(uint32_t sid, uint32_t nid)
{
	ServiceNode * snode = getServiceNode(sid);
	if (snode) {
		return snode->available(nid);
	}
	return false;
}

Node* NodeMgr::getAccessNode(int sid, const std::string& user_id)
{
	ServiceNode* snode = getServiceNode(sid);
	if (!snode) {
		return NULL;
	}
	return snode->getAccessNode(user_id);
}

int NodeMgr::getUserNodeId(int sid, const std::string& user_id)
{
	Node* node = getAccessNode(sid, user_id);
	if (node) {
		return node->getNid();
	}
	return -1;
}

int NodeMgr::getUserNodeSock(int sid, const std::string& user_id)
{
	Node* node = getAccessNode(sid, user_id);
	if (node) {
		return node->getsock();
	}
	return -1;
}

int NodeMgr::getUserNodeIdSock(int sid, const std::string& user_id, int& sockfd)
{
	Node* node = getAccessNode(sid, user_id);
	if (node) {
		sockfd = node->getsock();
		return node->getNid();
	}
	return -1;
}