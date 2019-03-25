#include "monitor_server.h"
#include "log_util.h"
#include "config_file_reader.h"
#include <IM.Basic.pb.h>
#include <IM.Server.pb.h>
#include <define.h>
#include <cstdarg>
#include <hash.h>
using namespace com::proto::basic;
using namespace com::proto::server;


MonitorServer::MonitorServer() :loop_(getEventLoop()), tcpService_(this, loop_) {
 
}

int MonitorServer::init()
{
    m_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
    m_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
    if (m_ip == "" || m_port == 0) {
        LOGD("not config ip or port");
        return -1;
    }
    return m_monitor.init(&tcpService_);

}

int MonitorServer::start() {
    LOGD("monitor server listen on %s:%d", m_ip.c_str(), m_port);
    if (tcpService_.listen(m_ip, m_port) == -1) {
        LOGD("listen [ip:%s,port:%d]fail", m_ip.c_str(), m_port);
        return -1;
    }
	tcpService_.run();
    return 0;
}

void MonitorServer::onData(int sockfd, PDUBase* base) {

    SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
    int cmd = spdu->command_id;
    switch (cmd) {
        case CID_CLUSTER_STATUS_REQ:
            clusterStatus(sockfd, *spdu);
            break;
        case CID_CONFIG_SET_REQ:
            configSet(sockfd, *spdu);
            break;
        default:
            m_monitor.handler(sockfd, *spdu);
            break;
    }
    delete base;
}

void MonitorServer::onEvent(int fd, ConnectionEvent event)
{
	if (event == Disconnected) {
		m_monitor.disconnectNode(fd);
		tcpService_.closeSocket(fd);
	}
}

int MonitorServer::clusterStatus(int sockfd, SPDUBase & base)
{
    ClusterState cluster_state;
    const auto& services_list = m_monitor.getServices();
    auto sit = services_list.begin();
    for (sit; sit != services_list.end(); sit++) {
        ::com::proto::server::ServiceState* service_state=cluster_state.add_cluster_state_list();
        service_state->set_service_type(sit->first);
        auto it = sit->second.begin();
        for (it; it != sit->second.end(); it++) {
            Node* node = *it;
            ::com::proto::server::ServerState* server_state = service_state->add_server_state_list();
            server_state->set_service_type(node->sid);
            server_state->set_node_id(node->nid);
            server_state->set_state(node->state);
            server_state->set_route_stragry(node->route_stragry);
            server_state->set_ip(node->ip);
            server_state->set_port(node->port);
            if (node->route_stragry == ROUTE_USER) {
                for (auto uit = node->route_users.begin(); uit != node->route_users.end(); uit++) {
                    server_state->add_user_list(*uit);
                }
            }
            else if (node->route_stragry == ROUTE_SLOT) {
                server_state->set_slots_str(node->slot_str);
            }

            if (node->state == NODE_UNABLE) {
                continue;
            }
            auto nid = node->connects.begin();

            for (nid; nid != node->connects.end(); nid++) {
                Node* cnode = &*nid;
                ::com::proto::server::NodeState* nodestate = server_state->add_conn_node_state_list();
                nodestate->set_node_id(cnode->nid);
                nodestate->set_service_type(cnode->sid);
                nodestate->set_state(cnode->state);
            }
        }
    }

    SPDUBase spdu;
    spdu.ResetPackBody(cluster_state, CID_CLUSTER_STATUS_RSP);
	tcpService_.Send(sockfd, spdu);
}

int MonitorServer::configSet(int sockfd, SPDUBase & base)
{
    ConfigSetReq config;
    if (!config.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "ConfigSetReq parse fail");
        return -1;
    }
    const auto& cmd = config.cmd();
    int size = cmd.size();
    std::string* cmd_list = new std::string[size];
    for (int i = 0; i < size; i++) {
        cmd_list[i] = cmd[i];
    }
    int res;
    if (!strcmp(cmd_list[0].c_str(), "add") && !strcmp(cmd_list[1].c_str(), "node")) {
        if (size != 5) {
            goto error;
        }
        addNode(sockfd,cmd_list, size);
    }
    else if (!strcmp(cmd_list[0].c_str(), "route") && !strcmp(cmd_list[1].c_str(), "set") && !strcmp(cmd_list[2].c_str(), "node")) {
        if (size <6) {
            goto error;
        }
        routeSet(sockfd, cmd_list, size);
    }
    else if (!strcmp(cmd_list[0].c_str(), "del") && !strcmp(cmd_list[1].c_str(), "node")) {
        if (size !=4) {
            goto error;
        }
        delNode(sockfd, cmd_list, size);
    }
    else if (!strcmp(cmd_list[0].c_str(), "stop") && !strcmp(cmd_list[1].c_str(), "node")) {
        if (size != 4) {
            goto error;
        }
        stopNode(sockfd, cmd_list, size);
    }
    else if (!strcmp(cmd_list[0].c_str(), "slot") && size == 2) {
        int user_id = atoi(cmd_list[1].c_str());
        int slot = dictGenHashFunction((const unsigned char*)&user_id, sizeof(int)) % CLUSTER_SLOTS;
        replyInfo(sockfd, "slot:%d",slot);
        return 0;
    }
    else {
        replyInfo(sockfd, "error cmd");
        return 0;
    }
    delete[] cmd_list;
    return 0;
error:
    replyInfo(sockfd, "cmd num error");
    return 0;
}

int MonitorServer::addNode(int sockfd,std::string * cmd, int size)
{
    std::string & sidName = cmd[2];
    int sid = getSid(sidName.c_str());
    if (sid == -1) {
        replyInfo(sockfd, "error sid");
        return -1;
    }
    std::string& ip = cmd[3];
    short port = atoi(cmd[4].c_str());
    int res = m_monitor.addNode(ip.c_str(), port, sid);
    if (res == -1) {
        replyInfo(sockfd, "add xml  fail");
    }
    else {
        replyInfo(sockfd, "add node[%d] success", res);
    }
    return 0;
}

int MonitorServer::delNode(int sockfd, std::string * cmd, int size)
{
    std::string & sidName = cmd[2];
    int sid = getSid(sidName.c_str());
    if (sid == -1) {
        replyInfo(sockfd, "error sid");
        return -1;
    }
    int nid = atoi(cmd[3].c_str());

    if (m_monitor.delNode(sid, nid) == -1) {
        replyInfo(sockfd, "del xml  fail");
    }
    else {
        replyInfo(sockfd, "del success");
    }
    return 0;
}

int MonitorServer::stopNode(int sockfd, std::string * cmd, int size)
{
    std::string & sidName = cmd[2];
    int sid = getSid(sidName.c_str());
    if (sid == -1) {
        replyInfo(sockfd, "error sid");
        return -1;
    }
    int nid = atoi(cmd[3].c_str());

    if (m_monitor.stopNode(sid, nid) == -1) {
        replyInfo(sockfd, "del xml  fail");
    }
    else {
        replyInfo(sockfd, "stop success");
    }
    return 0;
}

int MonitorServer::routeSet(int sockfd, std::string * cmd, int size)
{
    std::string & sidName = cmd[3];
    int sid = getSid(sidName.c_str());
    if (sid == -1) {
        replyInfo(sockfd, "error sid");
        return -1;
    }
    int nid = atoi(cmd[4].c_str());

    std::string& route_stragry = cmd[5];
    int stragry = 0;
    if (!strcmp(route_stragry.c_str(), "poll")) {
        stragry = ROUTE_POLL;
    }
    else if (!strcmp(route_stragry.c_str(), "hash")) {
        stragry = ROUTE_HASH;
    }
    else if (!strcmp(route_stragry.c_str(), "user")) {
        stragry = ROUTE_USER;
    }
    else if (!strcmp(route_stragry.c_str(), "slot") && size>=7) {
        stragry = ROUTE_SLOT;
    }
    else {
        replyInfo(sockfd, "route stragry set error,vlaue can be: poll,hash,slot or user");
        return -1;
    }
    if (stragry == ROUTE_SLOT) {
        if(m_monitor.routeSlotSet(sid, nid, cmd[6].c_str()) == -1) {
            replyInfo(sockfd, "route set xml  fail");
        }
        else {
            replyInfo(sockfd, "route set success");
        }
    }
    else{
        router_user_list_t  uli;
        if (stragry == ROUTE_USER) {
            for (int i = 6; i < size; i++) {
                uli.push_back(cmd[i].c_str());
            }
        }
        if (m_monitor.routeSet(sid, nid, stragry, uli) == -1) {
            replyInfo(sockfd, "route set xml  fail");
        }
        else {
            replyInfo(sockfd, "route set success");
        }
    }

return 0;
}

void MonitorServer::replyInfo(int sockfd,const char * content,...)
{
    va_list ap;
    va_start(ap, content);
    char reply[1024] = { 0 };
    vsnprintf(reply, 1024 - 1 , content, ap);
    va_end(ap);

    ConfigSetRsp rsp;
    rsp.set_reply(reply);
    SPDUBase spdu;
    spdu.ResetPackBody(rsp, CID_CONFIG_SET_RSP);
	tcpService_.Send(sockfd, spdu);
}

