#include "monitor_node.h"
#include <IM.Server.pb.h>
#include <IM.Basic.pb.h>
#include<log_util.h>
#include <config_file_reader.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cnode.h>
using namespace com::proto::server;
using namespace com::proto::basic;

#define REGISTER_RRT_TIME      4


int MonitorNode::pasreRouteSlotStr(const char * str, int len, Node * node)
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

void  MonitorNode::handler(int sockfd, SPDUBase& base) {

    int cid = base.command_id;
    switch (cid) {
        case CID_MONITOR_CLIENT_REGISTER_RSP:
            registRsp(sockfd,  base);
            delete &base;
            break;
        case CID_MASTER_BROADCAST_SERVER_INFO:
            srvinfoBroadcast(sockfd, base);
            delete &base;
            break;
        default:
            ServiceNode::handler(sockfd, base);
            break;
    }
}

MonitorNode::MonitorNode(){
    m_sid = SID_MONITOR;
    m_cur_sid=-1;
    m_cur_nid=-1;
    // m_epoch=0;
}

int MonitorNode::init()
{
    std::string monitor_ip = ConfigFileReader::getInstance()->ReadString(CONF_MONITOR_IP);
    int monitor_port = ConfigFileReader::getInstance()->ReadInt(CONF_MONITOR_PORT);
    if(monitor_ip=="" | monitor_port==0){
        LOGE("not config monitor ip or port");
    }

    m_monitorIP = monitor_ip;
    m_monitorPort = monitor_port;
    Node* node = m_pNodeMgr->addNode(m_sid, 0);
    node->setAddrs(m_monitorIP.c_str(), m_monitorPort);
    node->setState(NODE_CONNECT);
}

int MonitorNode::regist( Node* node)
{
    std::string tcpListen_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
    int tcpListen_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
    // m_cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
    TiXmlElement *root = ConfigFileReader::getInstance()->getRoot();
    const char* type = root->Attribute("type");
    m_cur_sid = getSid(type);
    if (0 == m_cur_sid) {
        LOGE("config type:%s error", type);
        return -1;
    }
    char sid[16] = { 0 };
    sprintf(sid, "%d", m_cur_sid);
    ConfigFileReader::getInstance()->setConfigValue(CONF_SERVICE_TYPE, sid);
    LOGD("regist info ip:%s,port:%d,sid:%s",tcpListen_ip.c_str(),tcpListen_port,sidName(m_cur_sid));

    //注册服务器
    RegisterServerReq register_server;

    register_server.set_server_ip(tcpListen_ip);
    register_server.set_server_port(tcpListen_port);
    register_server.set_service_type(m_cur_sid);
    SPDUBase pdu;
    pdu.ResetPackBody(register_server, CID_MONITOR_CLIENT_REGISTER_REQ);
    Send(node->getsock(), pdu);
    node->setState(NODE_REGIST);
    node->regist_time = time(0);
    return 0;
}

int MonitorNode::registRsp(int sockfd, SPDUBase& base)
{
    RegisterServerRsp rsp;
    if (!rsp.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "registRsp包解析错误");
        return -1;
    }
    int result = rsp.result_code();
    if(result!=1){
        LOGD("regist fail code(%d)", result);
        return -1;
    }
    /*if (result != SERVER_REG_SUCCESSED) {

      m_pNodeMgr->exit();
      return -1;
      }*/
    Node* node= getNodeBySock(sockfd);
    if(node){
        node->setState(NODE_ABLE);
    }
    else {
        LOGD("not find node");
    }
    LOGD("regist success");
    set_id(rsp.node_id());
    return true;
}

bool MonitorNode::srvinfoBroadcast(int sockfd, SPDUBase& base)
{
    ServerInfoBroadcast broadcast ;

    if (!broadcast.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "ServerInfoBroadcast包解析错误");
        return -1;
    }
    m_epoch = broadcast.epoch();

    const auto& server_list = broadcast.service_list();
    for (int i = 0; i < broadcast.service_list_size(); i++)
    {
        auto& srv_info = broadcast.service_list(i);
        int type = srv_info.service_type();
        ServiceNode* service_node = m_pNodeMgr->getServiceNode(type);

        for (int i = 0; i < srv_info.node_info_list_size(); i++)
        {
            auto& node_info = srv_info.node_info_list(i);
            uint32_t node_id = node_info.node_id();
            if (node_info.is_enable())//注册
            {
				m_pNodeMgr->addAccessNode(type, node_id);
				if (m_cur_nid == node_id) {//当前节点
					continue;
				}
                if (service_node) {
                    Node* node = service_node->findNode(node_id);
                    const char* ip = node_info.server_ip().c_str();
                    int port = node_info.server_port();
                    if (!node)//查找是否存在
                    {
                        node = service_node->addNode(type, node_id);
                        if (!node) {
                            continue;
                        }
                        LOGD("add node[sid:%s,nid_id:%u]", sidName(type), node_id);

                        node->setAddrs(ip, port);
                        node->setState(NODE_CONNECT);

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
                        if (strcmp(node->getIp(), ip) || node->getPort() != port) {
							node->setAddrs(ip, port);
                            node->setState(NODE_CONNECT);
                        }
                    }
                }
            }//节点不可用
            else {
				m_pNodeMgr->delAccessNode(type, node_id);
                if (m_cur_nid == node_id) {
                    LOGD("server will stop");
                    m_pNodeMgr->stop();
                    return true;
                }
                else{
                    if(service_node){
                        service_node->delNode(node_id);
                    }
                }
            }	

        }  
        if (service_node){
            service_node->recordMsgSlot();
        }
    }
	CNode::getInstance()->configureState(base);
    m_pNodeMgr->configureStateCb(base);
    return true;
}

void MonitorNode::checkState()
{
    std::list<Node*>::iterator it = m_nodes.begin();
    while (it != m_nodes.end()) {
        Node* pNode = *it;
        int state = (*it)->getState();
        if (state == NODE_CONNECT) {

            //	LOGD("node(sid:%u,nid(%u)", m_sid, pNode->getNodeId());

            int fd = connect(pNode->getIp(), pNode->getPort(), 0);
            if (fd != -1)
            {
                pNode->clear();
                pNode->setsock(fd);
                regist(pNode);

                //conn->setCache(old_conn->)
            }
            else {
                LOGW("connect node[service(%s),node(%u)] fail", sidName(m_sid), pNode->getNodeId());
            }
        }
        else if (state == NODE_REGIST) { // long time not recv regist rsp
            time_t cur_time = time(0);
            if (cur_time - pNode->regist_time > REGISTER_RRT_TIME) {
                regist(pNode);
            }
        }	
        ++it;
    }
    heartBeat();
}

void MonitorNode::set_id(uint32_t nid)
{
    m_cur_nid = nid;
    char id[10] = { 0 };
    sprintf(id, "%d", nid);
    ConfigFileReader::getInstance()->setConfigValue(CONF_NODE_ID, id);
}

