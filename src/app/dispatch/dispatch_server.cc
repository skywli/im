#include "dispatch_server.h"
#include "log_util.h"
#include "config_file_reader.h"
#include <algorithm>
#include <node_mgr.h>
#include <IM.Basic.pb.h>
#include <IM.Server.pb.h>
#include <IM.Basic.pb.h>
#include <hash.h>
using namespace com::proto::server;
using namespace com::proto::basic;


static int sec_recv_pkt;
static int total_recv_pkt;

static int sec_send_pkt;
 int total_send_pkts;
 int total_send_pkt;
DispatchServer::DispatchServer():loop_(getEventLoop()), tcpService_(this,loop_){
	m_pNode = CNode::getInstance();
}

int DispatchServer::init()
{
    m_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
    m_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
    if (m_ip == "" || m_port == 0) {
        LOGE("not config ip or port");
        return -1;
    }
   
	loop_->createTimeEvent(1000, Timer, this);
    if (NodeMgr::getInstance()->init(loop_, innerMsgCb, this) == -1) {
        return -1;
    }
	m_pNode->init(&tcpService_);
   // NodeMgr::getInstance()->setConfigureStateCb(configureStateCb, this);

}
void DispatchServer::reportOnliners()
{
	//printf("total recv %d pkts,every recv %d pkt, total send:%d ,every sec send:%d \n", total_recv_pkt, total_recv_pkt - sec_recv_pkt, total_send_pkts, total_send_pkts - sec_send_pkt);
	sec_recv_pkt = total_recv_pkt;
	sec_send_pkt = total_send_pkts;
}
void DispatchServer::Timer(int fd, short mask, void * privdata)
{
	DispatchServer* server = reinterpret_cast<DispatchServer*>(privdata);
	if (server) {
		server->reportOnliners();
		return;
	}

}
int DispatchServer::start() {
    LOGD("dispatch server listen on %s:%d", m_ip.c_str(), m_port);
    if (tcpService_.listen(m_ip, m_port) == -1) {
        LOGE("listen [ip:%s,port:%d]fail", m_ip.c_str(), m_port);
        return -1;
    }
    tcpService_.run();
    return 0;
}

//recv busi msg
void DispatchServer::onData(int sockfd, PDUBase* base) {
    SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
    int cmd = base->command_id;
    switch (cmd) {
        case CID_REGISTER_CMD_REQ:
            registCmdReq(sockfd, *spdu);
            break;
            //client cmd
        case CID_S2S_AUTHENTICATION_REQ:
        case CID_S2S_PING:
			m_pNode->handler(sockfd, *spdu);
            break;
        default:
			total_send_pkts++;
            processBusiMsg(sockfd, *spdu);
            break;
    }
    delete base;
}

void DispatchServer::onEvent(int fd, ConnectionEvent event)
{
	//busi disconn
	if (event == Disconnected) {
		m_pNode->setNodeDisconnect(fd);
		tcpService_.closeSocket(fd);
	}
	
}

int DispatchServer::getsock(int cmd, int user_id)
{
    return 0;
}
/* ................handler msg recving from busiserver.............*/
int DispatchServer::registCmdReq(int sockfd, SPDUBase & base)
{
    com::proto::server::RegisterCmdReq req;
    if (!req.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "authenticationReq°ü½âÎö´íÎó");
        return -1;
    }
    int sid = req.service_type();
    int nid = req.node_id();
	if (!m_pNode->existNode(sid, nid)) {
		LOGE("not find node[sid:%s,nid:%d],regist fail", sidName(sid), nid);
		//return -1;
	}
  /*  Node* node = findNode(sid, nid);
    if (node) {
        node->fd = sockfd;
        node->connected = true;
    }
    else {
        LOGE("not find node[sid:%s,nid:%d],regist fail", sidName(sid), nid);
        return -1;
    }*/
    LOGI("node[sid:%s,nid:%d] register success\n", sidName(sid), nid);
    auto cmd_list = req.cmd_list();
    auto it = cmd_list.begin();
    for (it; it != cmd_list.end(); it++) {
        m_cmd_map[*it] = sid;
    }
    return 0;
}

////handler msg recving from busiserver,transfer to connserver
int DispatchServer::processBusiMsg(int sockfd, SPDUBase & base)
{
    //send to busi server
    LOGD("recv msg from busi server cmd(%d)", base.command_id);
    int nid = base.node_id;
    if (NodeMgr::getInstance()->sendNode(SID_CONN, nid, base) == -1) {
        LOGE("send node[sid:%s,nid:%d]fail", sidName(SID_CONN), nid);
    }
    return 0;
}

/* ................handler msg recving from connserver.............*/
int DispatchServer::innerMsgCb(int sockfd, SPDUBase & base, void * arg)
{
	++total_recv_pkt;
    LOGD("recv msg from conn_server cmd(%d)", base.command_id);
    DispatchServer* server = reinterpret_cast<DispatchServer*>(arg);
    if (server) {
        if (server->dispatchMsg(sockfd, base) == -1) {
            LOGE("dispatch conn_server msg cmd(%d) fail", base.command_id);
        }
    }
    delete &base;
}

int DispatchServer::dispatchMsg(int sockfd, SPDUBase & base)
{
    int cmd = base.command_id;
    auto it = m_cmd_map.find(cmd);
    if (it != m_cmd_map.end()) {
        //not handler banlance 
       // Node* node = getAccessNode(it->second,base.terminal_token);
		int fd = -1;
        std::string user_id(base.terminal_token,sizeof(base.terminal_token));
		int nid=m_pNode->getUserNodeIdSock(it->second, user_id,fd);
        if (fd!=-1 && nid!=-1) {
            LOGD("dispatch cmd(%d) to node[sid:%s,nid:%d", cmd, sidName(it->second), nid);
            return tcpService_.Send(fd, base);
        }
        LOGE("no node handler cmd(%d) ", cmd);
    }
    LOGE("cmd(%d) not regist", cmd);
    return -1;
}

/* ................configure change cb.............*/
void  DispatchServer::configureStateCb(SPDUBase & base, void * arg)
{
    LOGD("configure change");
    DispatchServer* server = reinterpret_cast<DispatchServer*>(arg);
    if (server) {
        server->configureState(base);
    }
}

int DispatchServer::configureState(SPDUBase & base) {

}
#if 0
int DispatchServer::configureState(SPDUBase & base)
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
                if(!node){
                    LOGE("add node[sid:%d,nid:%d] fail",sidName(type),node_id);
					continue;
                }
                int stragry = node_info.route_strategy();
                node->stragry=stragry;
                if (stragry == ROUTE_USER) {
                       LOGD("node[sid:%s,nid:%d] route to user",sidName(type),node_id);
                    node->user_list.clear();
                    const auto& user_list = node_info.user_list();
                    for (int i = 0; i < user_list.size(); i++) {
                        node->user_list.push_back(user_list[i]);
                    }
                }
            }
            else {
                delNode(type, node_id);
            }
        }
    }
    return true;
}

DispatchServer::Node * DispatchServer::findNode(int sid, int nid)
{
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

int DispatchServer::disconnNode(int sockfd)
{
    auto sit = m_services.begin();
    while (sit != m_services.end()) {
        auto& node_list = sit->second;
        for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
            Node* node = *nit;
            if (node->fd == sockfd) {
                node->connected = false;
                return 0;
            }
        }
        ++sit;
    }
    return -1;
}

DispatchServer::Node* DispatchServer::getAccessNode(int sid,int user_id)
{
    int unable = 0;
    auto sit = m_services.find(sid);
    if (sit != m_services.end()) {
        auto& node_list = sit->second;
        for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
            Node* node = *nit;
            if (!node->connected) {
                ++unable;
                continue;
            }
            if (node->stragry == ROUTE_USER) {
                ++unable;
                for (auto uit = node->user_list.begin(); uit != node->user_list.end(); uit++) {
                    if (*uit == user_id) {
                        LOGD("user[uid:%d] route to sid:%s according to ROUTE_USER", user_id, sidName(sid));
                        return node;
                    }
                }
            }
        }
        int able = node_list.size() - unable;
        if (able && able > 0) {
            int index = dictGenHashFunction((const unsigned char*)&user_id,sizeof(int)) %  able;
            for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
                Node* node = *nit;
                if (!node->connected) {
                    continue;
                }
                if (!index) {
                    return node;
                }
                --index;
            }
        }
    }
    return NULL;
}

DispatchServer::Node * DispatchServer::addNode(int sid, int nid)
{
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

int DispatchServer::delNode(int sid, int nid)
{
    auto sit = m_services.begin();
    while (sit != m_services.end()) {
        auto& node_list = sit->second;
        for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
            Node* node = *nit;
            if (node->sid == sid && node->nid == nid) {
				LOGE("del node[sid:%s nid:%d] success", sidName(sid), nid);
                node_list.erase(nit);
                delete node;
                return 0;
            }
        }
        ++sit;
    }
    LOGE("del node[sid:%s nid:%d] fail", sidName(sid), nid);
    return -1;
}

int DispatchServer::setRouteStragry(Node * node, int stragry, const std::list<int>& user_list)
{
    node->stragry = stragry;
    node->user_list.clear();
    node->user_list = user_list;
}


#endif
