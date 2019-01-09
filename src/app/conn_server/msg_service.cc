#include "msg_service.h"
#include "log_util.h"
#include "config_file_reader.h"
#include "IM.Basic.pb.h"
#include <conn_service.h>
#include <node_mgr.h>
#include "log_util.h"
#include <string.h>
#include "IM.Loadbalance.pb.h"


using namespace com::proto::loadbalance;
using namespace com::proto::basic;

/*
* PDU解析
*/

MsgService::MsgService() {
	m_cur_dispatch = 0;
	m_pNode = CNode::getInstance();
}

void MsgService::OnAccept(SdEventLoop * eventLoop, int fd, void * clientData, int mask)
{
}


int MsgService::init(SdEventLoop* loop, ConnService*  conn )
{
	// 读取route配置信息，进行连接
	m_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
	m_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
	
	if (m_ip == "" || m_port == 0) {
		LOGE("not config inner ip or port");
		return -1;
	}
	m_loop = loop;
	m_connService = conn;
	TcpService::init(loop);
	 NodeMgr::getInstance()->init(loop,NULL,NULL);
     m_pNode->init(this);
	NodeMgr::getInstance()->setConnectionStateCb(connectionStateEventCb, this);
	return 0;
}

int MsgService::start() {

	LOGD("connect server listen on %s:%d", m_ip.c_str(), m_port);
	int res=TcpService::StartServer(m_ip, m_port);
	if (res == -1) {
		LOGE("listen [ip:%s,port:%d]fail", m_ip.c_str(), m_port);
	}
	return res;
}

void MsgService::connectionStateEventCb(NodeConnectedState* state, void* arg)
{
	int sid = state->sid;
	int sockfd = state->sockfd;
	MsgService* server = reinterpret_cast<MsgService*>(arg);
	if (server) {
		/*if (sid == SID_DISPATCH) {
			server->connectionStateEvent(sockfd, state, sid, nid);
			return;
		}*/
		 if (sid == SID_LOADBALANCE) {
			server->registLoadBalance(sockfd);
		}
		
	}
}

void MsgService::registLoadBalance(int sockfd) {
    LOGD("regist");
	Regist_CommunicationService regist;
	std::string ip = ConfigFileReader::getInstance()->ReadString(CONF_CONN_OUT_IP);
	short port = ConfigFileReader::getInstance()->ReadInt(CONF_CONN_OUT_PORT);
	regist.set_server_ip(ip);
	regist.set_server_port(port);
	SPDUBase spdu;
	spdu.ResetPackBody(regist, REGIST_COMMUNICATIONSERVICE);
	NodeMgr::getInstance()->sendsock(SID_LOADBALANCE, sockfd, spdu);
}

void MsgService::connectionStateEvent(int sockfd, int state, int sid, int nid)
{
	LOGD("sid:%s %s", sidName(sid), state == SOCK_CONNECTED ? "connect": "close");
	if (state == SOCK_CONNECTED) {
		SNode* node = new SNode;
		node->sid = sid;
		node->nid = nid;
		node->connected = true;
		node->sockfd = sockfd;
		m_nodes.push_back(node);
	}
	else {
		auto it = m_nodes.begin();
		while (it != m_nodes.end()) {
			SNode* node = *it;
			if (node->sid == sid && node->nid == nid) {
				m_nodes.erase(it);
				delete node;
				return;
			}
			++it;
		}
	}
}

int MsgService::recvClientMsg(int sockfd, PDUBase & base)
{
	SPDUBase pdu(base);
	pdu.node_id = m_connService->getNodeId();
	pdu.sockfd = sockfd;
	std::string  user_id(base.terminal_token, sizeof(base.terminal_token));
	int fd = m_pNode->getUserNodeSock(SID_DISPATCH, user_id);
	if (fd==-1) {
		LOGW("not find dispatch node");
		return -1;
	}
	if (Send(fd, pdu) == -1) {
		LOGE("send msg to dispatch fail");
		return -1;
	}
	return 0;
}

int MsgService::processInnerMsg(int sockfd, SPDUBase& base)
{
	std::string user_id(base.terminal_token, sizeof(base.terminal_token));
	LOGD("recv msg from dispatch server user_id:%s cmd(%d)", user_id.c_str(), base.command_id);
	PDUBase pdu(base);
	m_connService->recvBusiMsg(base.sockfd, pdu);
}

int MsgService::randomGetSock()
{
	auto it = m_nodes.begin();
	if (it != m_nodes.end()) {
	return (*it)->sockfd;
	}
	return -1;
}

//recv dispatch msg
void MsgService::OnRecv(int sockfd, PDUBase* base) {
	SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
	int cmd = base->command_id;
	switch (cmd) {
	case CID_S2S_AUTHENTICATION_REQ:
	case CID_S2S_PING:
		m_pNode->handler(sockfd, *spdu);
		break;
	default:
		processInnerMsg(sockfd, *spdu);
		break;
	}
	delete base;
}

void MsgService::OnConn(int sockfd) {
	/*Node* node = new Node(sockfd);
	m_nodes.push_back(node);*/
}

void MsgService::OnDisconn(int sockfd) {
	/*auto it = m_nodes.begin();
	while (it != m_nodes.end()) {
		SNode* node = *it;
		if (node->sid == sockfd ) {
			m_nodes.erase(it);
			delete node;
			return;
		}
		++it;
	}*/
	m_pNode->setNodeDisconnect(sockfd);
    CloseFd(sockfd);
}


