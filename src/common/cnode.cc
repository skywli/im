#include <cnode.h>
#include <log_util.h>
#include "config_file_reader.h"
#include <IM.Basic.pb.h>
#include <IM.Server.pb.h>
#include <IM.Basic.pb.h>
#include <hash.h>
#include <tcp_service.h>
using namespace com::proto::server;
using namespace com::proto::basic;

CNode::CNode()
{
	m_pNet = NULL;
	memset(msg_slots, 0, sizeof(msg_slots));
}

CNode::~CNode()
{
}

CNode * CNode::getInstance()
{
	static CNode node;
	return &node;
}

bool CNode::handler(int sockfd, SPDUBase& base)
{
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

bool CNode::authenticationReq(int sockfd, SPDUBase& base)
{
	com::proto::server::AuthenticationReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "authenticationReq°ü½âÎö´íÎó");
		return -1;
	}
	uint16_t sid = req.service_type();
	uint32_t nid = req.node_id();
	LOGD("recv node[sid:%s,nid:%d] authenticationReq", sidName(sid), nid);
	//only  after recving broadcast,we can consider the node able;
	Node* node = findNode(sid, nid);
	if (!node) {
		return false;
	}
	node->fd = sockfd;
	node->connected = true;
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
	return true;
}

bool CNode::pingReq(int sockfd, SPDUBase& base)
{
	PingReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "authenticationReq°ü½âÎö´íÎó");
		return false;
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
	return true;
}

int CNode::pasreRouteSlotStr(const char * str, int len, Node * node)
{
	char* tmp = (char*)malloc(len + 1);
	if (!tmp) {
		return -1;
	}
	memset(tmp, 0, len + 1);
	strcpy(tmp, str);
	char* p = tmp;
	char* pos = tmp;
	int segment = 0;
	while (*pos) {
		if (*pos != ' ') {
			*p++ = *pos;
		}
		pos++;
	}
	*p = '\0';
	memset(node->slots, 0, sizeof(node->slots));
	//printf("str:%s\n", tmp);
	pos = tmp;
	char* begin = NULL;;
	char* end = NULL;;
	int right;
	int left;
	int flag = 0;
	int res = 0;
	while (*pos) {
		if (*pos == ',') {
			pos++;
		}
		if (*pos == '[') {
			begin = pos + 1;
			while (*pos) {
				if (*pos == '.') {
					*pos = '\0';
					left = atoi(begin);
					if (*(pos + 1) != '.' || *(pos + 2) != '.') {
						LOGE("format error");
						goto end;
					}
					pos += 3;
					begin = pos;
					if (!(*pos >= '1'  && *pos <= '9')) {
						LOGE("format error");
						goto end;
					}
					flag = 1;

				}
				if (*pos == ']') {
					if (!flag) {
						LOGE("format error");
						goto end;
					}
					flag = 0;
					end = pos;
					*pos = '\0';
					right = atoi(begin);
					pos++;//skip ]
					break;
				}
				pos++;
			}
			if (!end) {
				LOGE("not find ] ");
				goto end;
			}
			if (right<left) {
				LOGE("right lower left");
				goto end;
			}
			end = NULL;
			if (right>255) {
				LOGE("slot should low 255");
				goto end;
			}
			//	printf("left:%d,right:%d\n", left, right);
			for (int i = left; i <= right; i++) {
				int index = i / 8;
				int bit = i % 8;
				node->slots[index] |= 1 << bit;
			}
			continue; //skip tail pos++
		}
		else {
			begin = pos;
			if (!(*begin >= '1'  && *begin <= '9')) {
				return -1;
			}
			while (*pos) {
				if (*pos == ',') {
					*pos = '\0';
					left = atoi(begin);
					pos++;//skip ,
					break;
				}
				pos++;
			}
			//only one item
			if (!*pos) {
				left = atoi(begin);
			}
			//printf("signal:%d\n", left);
			int index = left / 8;
			int bit = left % 8;
			node->slots[index] |= 1 << bit;
			continue;
		}
		pos++;
	}
	free(tmp);
	node->slot_str = str;
	return 0;
end:
	free(tmp);
	return -1;
}

int CNode::init(TcpService * net)
{
	m_pNet = net;
	return 0;
}

int CNode::configureState(SPDUBase & base)
{
	ServerInfoBroadcast broadcast;

	if (!broadcast.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "ServerInfoBroadcast°ü½âÎö´íÎó");
		return -1;
	}

	const auto& server_list = broadcast.service_list();
	for (int i = 0; i < broadcast.service_list_size(); i++)
	{
		auto& srv_info = broadcast.service_list(i);
		int type = srv_info.service_type();

		for (int i = 0; i < srv_info.node_info_list_size(); i++)
		{
			auto& node_info = srv_info.node_info_list(i);
			uint32_t node_id = node_info.node_id();
			if (node_info.is_enable())//×¢²á
			{
				Node* node = addNode(type, node_id);
				if (!node) {
					LOGE("add node[sid:%d,nid:%d] fail", sidName(type), node_id);
					continue;
				}
				int stragry = node_info.route_strategy();
				node->stragry = stragry;
				if (stragry == ROUTE_USER) {
					LOGD("node[sid:%s,nid:%d] route to user", sidName(type), node_id);
					node->user_list.clear();
					const auto& user_list = node_info.user_list();
					for (int i = 0; i < user_list.size(); i++) {
						node->user_list.push_back(user_list[i]);
					}
				}
				else if (stragry == ROUTE_SLOT) {
					if (pasreRouteSlotStr(node_info.slots().c_str(), strlen(node_info.slots().c_str()), node) == -1) {
						LOGE("parse slot str fail");
					}
				}
			}
			else {
				delNode(type, node_id);
			}
		}
	}
	recordMsgSlot();
	return true;
}


bool CNode::existNode(int sid, int nid)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	if (findNode(sid, nid)) {
		return true;
	}
	return false;
}

int CNode::setNodeConnect(int sid, int nid, int sockfd)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	Node* node;
	if ((node=findNode(sid, nid))!=NULL) {
		node->fd = sockfd;
		node->connected = true;
		return 0;
	}
	return -1;
}

CNode::Node * CNode::findNode(int sid, int nid)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	auto sit = m_services.begin();
	while (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			Node* node = *nit;
			if (node->sid == sid && node->nid == nid) {
				return node;
			}
		}
		++sit;
	}
	return NULL;
}

int CNode::setNodeDisconnect(int sockfd)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	auto sit = m_services.begin();
	while (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			Node* node = *nit;
			if (node->fd == sockfd) {
				node->connected = false;
				node->fd = -1;
				return 0;
			}
		}
		++sit;
	}
	return -1;
}

CNode::Node* CNode::getAccessNode(int sid, const std::string& user_id)
{
	int unable = 0;
	auto sit = m_services.find(sid);
	if (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); ) {
			Node* node = *nit;
			if (node->unable) {
				nit =node_list.erase(nit);
				delete node;
				continue;
			}
			if (!node->connected) {
				++unable;
				nit++;
				continue;
			}
			if (node->stragry == ROUTE_USER) {
				++unable;
				for (auto uit = node->user_list.begin(); uit != node->user_list.end(); uit++) {
					if (*uit == user_id) {
						LOGD("user[uid:%s] route to node[sid:%s nid:%d] according to ROUTE_USER", user_id.c_str(), sidName(sid),node->nid);
						return node;
					}
				}
			}
			nit++;
		}
		const char* p = user_id.c_str();
		if (sid == SID_MSG) {
			//slot
			int slot = dictGenHashFunction((const unsigned char*)p, user_id.length()) % CLUSTER_SLOTS;
			Node* node = msg_slots[slot];
			if (node) {
				//LOGD("user[uid:%s] route to node[sid:%s,nid:%d] according to ROUTE_SLOTS", user_id.c_str(), sidName(sid),node->nid);
				return node;
			}
			LOGE("user[uid:%s] route to sid:%s according to ROUTE_SLOTS  not find node", user_id.c_str(), sidName(sid));
			return NULL;
		}

		
		int able = node_list.size() - unable;
		if (able && able > 0) {
			int index = dictGenHashFunction((const unsigned char*)&user_id, sizeof(int)) % able;
			for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
				Node* node = *nit;
				if (!node->connected) {
					continue;
				}
				if (!index) {
					//LOGD("user[uid:%s] route to sid:%s according to ROUTE_HASH", user_id.c_str(), sidName(sid));
					return node;
				}
				--index;
			}
		}
	}
	LOGE("user[uid:%s] route to sid:%s  not find node", user_id.c_str(), sidName(sid));
	return NULL;
}

int CNode::getUserNodeId(int sid,const std::string& user_id)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	Node* node = getAccessNode(sid, user_id);
	if (node) {
		return node->nid;
	}
	return -1;
}

int CNode::getUserNodeSock(int sid, const std::string& user_id)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	Node* node = getAccessNode(sid, user_id);
	if (node) {
		return node->fd;
	}
	return -1;
}

int CNode::getUserNodeIdSock(int sid, const std::string& user_id,int& sockfd)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	Node* node = getAccessNode(sid, user_id);
	if (node) {
		sockfd = node->fd;
		return node->nid;
	}
	return -1;
}

CNode::Node * CNode::addNode(int sid, int nid)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	auto sit = m_services.find(sid);
	if (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			if ((*nit)->nid == nid) {
				return *nit;
			}
		}
		Node * node = new Node(sid, nid);
		node_list.push_back(node);
		return node;
	}
	else {
		std::list<Node*> node_list;
		Node * node = new Node(sid, nid);
		node_list.push_back(node);
		m_services[sid] = node_list;
		return node;
	}
}

int CNode::delNode(int sid, int nid)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	auto sit = m_services.begin();
	while (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			Node* node = *nit;
			if (node->sid == sid && node->nid == nid) {
				LOGE("del node[sid:%s nid:%d] success", sidName(sid), nid);
				node->unable = true;
				//node_list.erase(nit);
				//delete node;
				return 0;
			}
		}
		++sit;
	}
	LOGE("del node[sid:%s nid:%d] fail", sidName(sid), nid);
	return -1;
}

int CNode::setRouteStragry(Node * node, int stragry, const std::list<std::string>& user_list)
{
	node->stragry = stragry;
	node->user_list.clear();
	node->user_list = user_list;
}

int CNode::recordMsgSlot()
{
	std::lock_guard<std::recursive_mutex> lock_1(m_services_mutex);
	memset(msg_slots, 0, sizeof(msg_slots));
	auto sit = m_services.find(SID_MSG);
	if (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			if ((*nit)->stragry == ROUTE_SLOT) {
				for (int i = 0; i<CLUSTER_SLOTS/8; i++) {
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
}
