#include "client.h"
#include "log_util.h"
#include "config_file_reader.h"
#include <node_mgr.h>
#include "IM.Server.pb.h"
#include <IM.Basic.pb.h>
#include <algorithm>
#include <arpa/inet.h>

using namespace com::proto::basic;
using namespace com::proto::server;
static int maxclients = 1000000;


#define CONFIG_FDSET_INCR  128


static const char * nodeState(int state)
{
    if (state == NODE_UNABLE) {
        return "unable";
    }
    else if (state == NODE_CONNECT) {
        return "connect";
    }
    else if (state == NODE_AUTH) {
        return "auth";
    }
    else if (state == NODE_REGIST) {
        return "regist";
    }
    else if (state == NODE_ABLE) {
        return "able";
    }
    else if (state == NODE_TIMEOUT) {
        return "timeout";
    }
    else {
        return "unknow state";
    }
}
MonitorClient::MonitorClient(){
}

int MonitorClient::init()
{
    m_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
    m_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
    if (m_ip == "" || m_port == 0) {
        LOGD("not config out ip or port");
        return -1;
    }
    Connect(m_ip.c_str(), m_port);
}

int MonitorClient::start() {
    return 0;
}

int MonitorClient::processCmd(int argc, char * argv[])
{
    int res = -1;
    if (!strcmp(argv[0], "status")) {
        SPDUBase spdu;
        spdu.command_id = CID_CLUSTER_STATUS_REQ;
        return Send(spdu);
    }
    if (!strcmp(argv[0], "add") && !strcmp(argv[1], "node")  && argc == 5) {
        res = 0;
    }
    else if (!strcmp(argv[0], "route") && !strcmp(argv[1], "set") && !strcmp(argv[2], "node")  && argc >= 6) {
        res = 0;
    }
    else if (!strcmp(argv[0], "del") && !strcmp(argv[1], "node")  && argc==4) {
        res = 0;
    }
	else if (!strcmp(argv[0], "stop") && !strcmp(argv[1], "node") && argc == 4) {
		res = 0;
	}
	else if (!strcmp(argv[0], "slot")  && argc == 2) {
		res = 0;
	}
    if (!res) {
        ConfigSetReq req;
        for (int i = 0; i < argc; i++) {
            req.add_cmd(argv[i]);
        }
        if (SendProto(req, CID_CONFIG_SET_REQ) < 0) {
            printf("send fail\n");
            return -1;
        }
    }
    if(res==-1){
        printf("cmd error\n");
    }
    return res;
}

int MonitorClient::read()
{
    SPDUBase  base;
    if (Read(base) < 0) {
        printf("read fail");
        return 0;
    }
    OnRecv(&base);
}

int MonitorClient::clusterState( SPDUBase & base)
{
    ClusterState cluster_state;

    if (!cluster_state.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "cluster_state parse fail");
        return -1;
    }
    auto& cluster = cluster_state.cluster_state_list();
    auto cit = cluster.begin();
    int i = 0;
    for (cit; cit != cluster.end(); cit++) {
		printf("\n");
		const ServiceState& service_state = *cit;
		printf("		%d: sid:%s\n", ++i, sidName(service_state.service_type()));
		const auto& server_state_list = service_state.server_state_list();
		for (int j = 0; j < server_state_list.size(); j++) {
			const ServerState& state = server_state_list[j];
			const auto& ip = state.ip();
			int port = state.port();
			std::string addr = ip + ":"+std::to_string(port);
			printf("			[%d] nid:%d  addr:%s route stagry:%d %s\n", j+1,  state.node_id(), addr.c_str(),state.route_stragry(), nodeState(state.state()));
			//	printf("			route stagry:%d\n",state.route_stragry());
			if (state.route_stragry() == 3) {
				printf("			slots:%s\n", state.slots_str().c_str());
			}
			else if (state.route_stragry() == 4) {
				auto& user_list = state.user_list();
				int k = 0;
				if (user_list.size() > 0) {
					printf("                        users:%d", user_list.size());

					for (auto uit = user_list.begin(); uit != user_list.end(); uit++) {
						if (!(k++ % 5)) {
							printf("\n");
							printf("		            ");
						}
						printf("	 %s", uit->c_str());
					}
					printf("\n");
				}
			}
			auto& node_state_list = state.conn_node_state_list();
			if (node_state_list.size() > 0) {
				printf("			connect node:%d\n", node_state_list.size());
			}
			for (int j = 0; j < node_state_list.size(); j++) {
				const NodeState& state = node_state_list[j];
				printf("			    [%d] sid:%s nid:%d %s\n", j+1, sidName(state.service_type()), state.node_id(), nodeState(state.state()));
			}
			//	printf("\n");
		}
    }
}

int MonitorClient::configSetRsp( SPDUBase & base)
{
    ConfigSetRsp rsp;

    if (!rsp.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "ConfigSetRsp parse fail");
        return -1;
    }
    printf("\n		%s\n", rsp.reply().c_str());
    return 0;
}

void MonitorClient::OnRecv( PDUBase* _base) {
    SPDUBase* spdu = dynamic_cast<SPDUBase*>(_base);
    int cmd = spdu->command_id;
    switch (cmd) {
        case CID_CLUSTER_STATUS_RSP:
            clusterState( *spdu);
            break;
        case CID_CONFIG_SET_RSP:
            configSetRsp( *spdu);
            break;
    }


}

void MonitorClient::OnConnect() {

}

void MonitorClient::OnDisconnect() {

}

