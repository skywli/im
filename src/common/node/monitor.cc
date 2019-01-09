
#include <monitor.h>
#include <log_util.h>
#include <IM.Server.pb.h>
#include <IM.Basic.pb.h>
#include <config_file_reader.h>
#include <define.h>
using namespace com::proto::server;
using namespace com::proto::basic;

const char * nodeState(int state)
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

int SplitString(std::string & src, router_user_list_t& user_list)
{
	auto start = src.begin();
	auto it = src.begin();
	for (; it != src.end(); it++) {
		if (*it == ' ') {
			start++;
			continue;
		}
		if (*it == ',') {
			user_list.push_back(std::string(start, it).c_str());
			start = it + 1;
		}
	}
	if (!src.empty()) {
		user_list.push_back(std::string(start, it).c_str());
	}
}
void  Monitor::handler (int sockfd, SPDUBase& base) {
	uint16_t cid = base.command_id;
	switch (cid) {
	case CID_MONITOR_CLIENT_REGISTER_REQ:
		registReq(sockfd, base);
		break;
	case CID_S2S_PING:
		pingReq(sockfd, base);
		break;
	default:
        LOGD("unknown cmd(%d)",cid);
		break;
	}
}

Monitor::Monitor(){
	m_sid=SID_MONITOR;
	m_server = NULL;
	m_epoch = 1000;
    m_nid=0;
	m_cur_nid = 0;
}

int Monitor::init(TcpService* server)
{
	m_server = server;

	TiXmlElement *root = ConfigFileReader::getInstance()->getRoot();
	TiXmlElement* services;
	if (!ConfigFileReader::getInstance()->getNodePointerByName(root, CONF_SERVICEES, services)) {
		LOGE("not find services elem");
		return -1;
	}
	int i = 0;
	std::list<const char*> service_list;
	for (TiXmlElement* service = services->FirstChildElement(); service != NULL; service = service->NextSiblingElement()) {
		const char* type,*name,*ip;
		short port;
		int nid;
		int stragry = ROUTE_HASH;
		std::string stragry_info;
		type = service->Attribute("type");
	//	printf("%s,name:%s\n", elem->Value(), type);
		for (TiXmlElement* node = service->FirstChildElement(); node != NULL; node = node->NextSiblingElement()) {
			name = node->Attribute("name");
			//printf("%s,name:%s\n", elem->Value(), name);
			for (TiXmlElement* elem = node->FirstChildElement(); elem != NULL; elem = elem->NextSiblingElement())
			{
				TiXmlNode* e1 = elem->FirstChild();
				if (!strcmp("ip", elem->Value())) {
					ip = e1->ToText()->Value();

				}
				else if (!strcmp("port", elem->Value())) {
					port = atoi(e1->ToText()->Value());
				}
				else if (!strcmp("id", elem->Value())) {
					nid = atoi(e1->ToText()->Value());
				}
				else if (!strcmp("route", elem->Value())) {
					stragry = atoi(elem->Attribute("stragry"));
					stragry_info = e1->ToText()->Value();
					printf("stragry:%d,%s\n", stragry, stragry_info.c_str());
				}
			}
			Node* nd = addNode(ip, port, getSid(type), nid, name);
			if (!nd) {
				LOGE("add node ip:%s port:%d nid:%d fail", ip, port, nid);
				return -1;
			}
			nd->route_stragry = stragry;
			if (stragry == ROUTE_USER) {
				SplitString(stragry_info, nd->route_users);
			}
			else if (stragry == ROUTE_SLOT) {
				if (pasreRouteSlotStr(stragry_info.c_str(), strlen(stragry_info.c_str()), nd) == -1) {
					LOGE("parse slot str fail");
					return -1;
				}
			}
		}

	}
	m_server->CreateTimer(15000, Timer, this);
	return 0;
}

void Monitor::Timer(int fd, short mask, void * privdata)
{
	Monitor* server = reinterpret_cast<Monitor*>(privdata);
	if (!server) {
		return;
	}
	server->checkState();
}

void Monitor::checkState()
{
	auto sit = m_services.begin();
	for (sit; sit != m_services.end(); sit++) {
		auto it = sit->second.begin();
		for (it; it != sit->second.end(); it++) {
			Node* node = *it;
			if (node->ping) {
				if (time(0) > node->ping + 20) {
					if (node->state == NODE_ABLE) {
						node->state = NODE_TIMEOUT;
					}
				}
			}
		}
	}
}

const std::map<int, std::list<Node*>>& Monitor::getServices()
{
	return m_services;
}



int Monitor::registReq(int sockfd,SPDUBase& base)
{
    LOGD("Regist req");
	RegisterServerReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "registReq包解析错误");
		return -1;
	}
	const std::string& ip = req.server_ip();
	short port = req.server_port();
	int sid = req.service_type();
	Node* node= findNode(ip.c_str(),port,sid);
	int result = 0;
	if (node) {
		result = 1;
		m_epoch++;
		node->fd = sockfd;
		node->state = NODE_ABLE;
		node->enable = true;
		nodeInfo(node, "regist success");
	}
	else {
		LOGE("regist ip:%s,port:%d fail sid:%s", ip.c_str(), port,sidName(sid));
	}
	RegisterServerRsp rsp;
	if (node) {
		rsp.set_node_id(node->nid);
	}
	
	rsp.set_result_code(result);
	base.ResetPackBody(rsp, CID_MONITOR_CLIENT_REGISTER_RSP);
	m_server->Send(sockfd, base);
    if(node){
	    srvinfoBroadcast();
    }
}

bool Monitor::pingReq(int sockfd, SPDUBase& base)
{
	base.command_id = CID_S2S_PONG;
	PingReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "pingReq包解析错误");
		return -1;
	}
	int sid = req.service_type();
	int nid = req.node_id();
	Node* node = findNode(sid, nid);
	if (!node) {
		LOGE("node[sid:%d,nid:%d] not regist", sid, nid);
		return false;
	}
	node->ping = time(0);
	node->state = NODE_ABLE;
	node->connects.clear();
	int epoch = req.epoch();
	auto& state_list = req.state_list();
    LOGI("                        node[sid:%s,nid:%d] ping",sidName(sid),nid);
	auto it = state_list.begin();
	while (it != state_list.end()) {
		LOGI("                        node[sid:%s,nid:%d] %s                              ", sidName(it->service_type()), it->node_id(), nodeState(it->state()));
		Node cnode;
		cnode.sid = it->service_type();
		cnode.nid = it->node_id();
		cnode.state = it->state();
		node->connects.push_back(cnode);
        ++it;
	}
	
	LOGI("\n\n");
	PongRsp rsp;
	rsp.set_service_type(m_sid);
	rsp.set_node_id(m_nid);
	base.ResetPackBody(rsp, CID_S2S_PONG);
	m_server->Send(sockfd, base);
	
	if (epoch != m_epoch) {
		srvinfoBroadcast(sockfd);
	}
	return false;
}

int Monitor::srvinfoBroadcast(int flag)
{
	ServerInfoBroadcast broadcast;
	broadcast.set_epoch(m_epoch);
	auto sit = m_services.begin();
	for (sit; sit != m_services.end(); sit++) {
       
		auto server_list_item = broadcast.add_service_list();
		server_list_item->set_service_type(sit->first);
		auto it = sit->second.begin();
		for (it; it != sit->second.end(); it++) {
			if ((*it)->state != NODE_ABLE) {
				continue;
			}
			auto node_list_item = server_list_item->add_node_info_list();
			node_list_item->set_is_enable((*it)->enable);
			node_list_item->set_node_id((*it)->nid);
			node_list_item->set_server_ip((*it)->ip);
			node_list_item->set_server_port((*it)->port);
			node_list_item->set_service_type((*it)->sid);
			node_list_item->set_route_strategy((*it)->route_stragry);
			if ((*it)->route_stragry == ROUTE_USER) {
				for (auto uit = (*it)->route_users.begin(); uit != (*it)->route_users.end(); uit++) {
					node_list_item->add_user_list(*uit);
				}
			}
			else if ((*it)->route_stragry == ROUTE_SLOT) {
				node_list_item->set_slots((*it)->slot_str);
			}
			
		}
		
	}
	SPDUBase pdu;
	pdu.ResetPackBody(broadcast, CID_MASTER_BROADCAST_SERVER_INFO);
	if (flag == 0) {
		sit = m_services.begin();
		for (sit; sit != m_services.end(); sit++) {
			auto it = sit->second.begin();
			for (it; it != sit->second.end(); it++) {
				if ((*it)->state == NODE_ABLE) {
					m_server->Send((*it)->fd, pdu);
				}
            }
		}
	}
	else {
		m_server->Send(flag, pdu);
	}
	
}

Node* Monitor::addNode(const char* ip, short port,int sid, int nid,const char* name)
{
	Node* node = new Node;
	auto sit = m_services.find(sid);
	if (sit != m_services.end()) {
		auto it = sit->second.begin();
		for (it; it != sit->second.end();it++) {
			if (!strcmp((*it)->ip, ip) && (*it)->port == port) {
				delete node;
				return NULL;
			}
		}
		sit->second.push_back(node);
	}
	else {
		std::list<Node*> node_list;
		node_list.push_back(node);
		m_services[sid] = node_list;
	}

	node->port = port;
	node->sid = sid;
	node->nid=nid;
	strcpy(node->name, name);
	strcpy(node->ip, ip);
	node->ping = 0;
	LOGI("add node[name:%s ,ip:%s,port:%d,sid:%d,nid:%d]",name, ip,port ,sid,nid);

	if (m_cur_nid < nid) {
		m_cur_nid = nid;
	}
	return node;
}

int Monitor::pasreRouteSlotStr(const char * str, int len,Node * node)
{
	char* tmp = (char*)malloc(len + 1);
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

Node* Monitor::findNode(const char* ip, short port, int sid)
{
	auto sit = m_services.find(sid);
	if (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			if (!strcmp((*nit)->ip,ip) && (*nit)->port == port) {
				return *nit;
			}
		}
	}
	return NULL;
}

Node * Monitor::findNode(int sid, int nid)
{
	auto sit = m_services.find(sid);
	if (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			if ((*nit)->sid == sid && (*nit)->nid == nid) {
				return *nit;
			}
		}
	}
	return NULL;
}

int Monitor::disconnectNode(int sockfd)
{
	auto sit = m_services.begin();
	while (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			if ((*nit)->fd == sockfd) {
				(*nit)->fd = -1;
				(*nit)->state = NODE_UNABLE;
				(*nit)->connects.clear();
				nodeInfo(*nit, "disconnect");
				break;
			}
		}
        ++sit;
	}
}

void Monitor::nodeInfo(Node * node, const char * tip)
{
	LOGI("node[ip:%s,port:%d ,sid:%s,nid:%d %s", node->ip, node->port,sidName(node->sid),node->nid,tip);
}

int Monitor::addNode(const char * ip, short port, int sid)
{
	int nid = ++m_cur_nid;
	const char* sid_name = sidName(sid);
	char name[32] = { 0 };
	sprintf(name, "%s%d", sid_name, nid);
	if (addNode(ip, port, sid, nid, name)) {
		router_user_list_t uli;
		if (XMLAddNode(ip, port, sid, nid, name, ROUTE_HASH, uli) == -1) {
			LOGE("XML add node[sid:%s] fail", sidName(sid));
			return -1;
		}
        return nid;
	}
	return -1;
}

int Monitor::stopNode(int sid, int nid)
{
	auto sit = m_services.begin();
	while (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			Node* node = *nit;
			if (node->sid == sid && node->nid == nid) {
				node->enable = false;
				srvinfoBroadcast();
				return 0;
			}
		}
		++sit;
	}
	LOGE("not find  node[sid:%s nid:%d] fail", sidName(sid), nid);
	return -1;
}

int Monitor::delNode(int sid, int nid)
{
	auto sit = m_services.begin();
	while (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			Node* node = *nit;
			if (node->sid == sid && node->nid==nid) {
				node_list.erase(nit);
				delete node;
				if (XMLDelNode(sid, nid) == -1) {
					LOGE("del node[sid:%s,nid:%d fail]", sidName, nid);
					return -1;
				}
				return 0;
			}
		}
		++sit;
	}
	LOGE("not find  node[sid:%s nid:%d] fail", sidName(sid), nid);
	return -1;
}

int Monitor::routeSet(int sid, int nid, int route_stragry, router_user_list_t& user_list)
{
	auto sit = m_services.begin();
	while (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			Node* node = *nit;
			if (node->sid == sid && node->nid == nid) {
				node->route_stragry = route_stragry;
				node->route_users.clear();
				if (route_stragry == ROUTE_USER) {
					node->route_users = user_list;
				}
				
				if (XMLRouteNode(sid, nid,route_stragry,user_list) == -1) {
					LOGE("set route node[sid:%s,nid:%d ]fail", sidName, nid);
					return -1;
				}
                srvinfoBroadcast();
				return 0;
			}
		}
		++sit;
	}
	LOGE("not find  node[sid:%s nid:%d] fail", sidName(sid), nid);
    return -1;
}

int Monitor::routeSlotSet(int sid, int nid, const char * str)
{
	int len = strlen(str);
	auto sit = m_services.begin();
	while (sit != m_services.end()) {
		auto& node_list = sit->second;
		for (auto nit = node_list.begin(); nit != node_list.end(); nit++) {
			Node* node = *nit;
			if (node->sid == sid && node->nid == nid) {
				node->route_stragry = ROUTE_SLOT;
				node->route_users.clear();
				if (pasreRouteSlotStr(str, len, node) == -1) {
					return -1;
				}
				if (XMLRouteNode(sid, nid, ROUTE_SLOT,str) == -1) {
					LOGE("set route node[sid:%s,nid:%d ]fail", sidName, nid);
					return -1;
				}
				srvinfoBroadcast();
				return 0;
			}
		}
		++sit;
	}
	LOGE("not find  node[sid:%s nid:%d] fail", sidName(sid), nid);
	return -1;
}

int Monitor::XMLAddNode(const char * ip, short port, int sid, int nid, const char * name,int route_stragry, router_user_list_t& user_list)
{
	TiXmlElement *root = ConfigFileReader::getInstance()->getRoot();
	TiXmlElement* services;
	if (!ConfigFileReader::getInstance()->getNodePointerByName(root, CONF_SERVICEES, services)) {
		LOGE("not find services elem");
		return -1;
	}
	const char* sidname = sidName(sid);
	if (strcmp(sidname, "null")) {
		 TiXmlNode* service = NULL; //初始化
		while(service = services->IterateChildren("service", service))
		{
			const char* type = service->ToElement()->Attribute("type");
			LOGD("type:%s", type);
			if (!strcmp(type, sidname)) {
				 TiXmlElement* ser = service->ToElement();
				TiXmlElement* node = new TiXmlElement("node");
				node->SetAttribute("name", name);

				TiXmlElement* xml_ip = new TiXmlElement("ip");
				xml_ip->LinkEndChild(new TiXmlText(ip));
				node->LinkEndChild(xml_ip);

				TiXmlElement* xml_port = new TiXmlElement("port");
				char c_port[16] = { 0 };
				sprintf(c_port, "%d", port);
				xml_port->LinkEndChild(new TiXmlText(c_port));
				node->LinkEndChild(xml_port);

				TiXmlElement* xml_id = new TiXmlElement("id");
				char c_id[16] = { 0 };
				sprintf(c_id, "%d", nid);
				xml_id->LinkEndChild(new TiXmlText(c_id));
				node->LinkEndChild(xml_id);

				TiXmlElement* xml_stragry = new TiXmlElement("route");
				char c_stragry[16] = { 0 };
				sprintf(c_stragry, "%d", route_stragry);

				xml_stragry->SetAttribute("stragry", c_stragry);
				
				if (route_stragry == ROUTE_USER) {
					char users[8192] = { 0 };
					int len = 0;
					
					for (auto it = user_list.begin(); it != user_list.end(); it++) {
						len += snprintf(users+len, 8192 - len - 1, "%s,", it->c_str());
					}
					users[len - 1] = '\0';
					xml_stragry->LinkEndChild(new TiXmlText(users));
				}
				else {
					xml_stragry->LinkEndChild(new TiXmlText("*"));
				}
				node->LinkEndChild(xml_stragry);
				
				ser->LinkEndChild(node);
				if (ConfigFileReader::getInstance()->save() == -1) {
					LOGE("save fail");
					return -1;
				}
				return 0;
			}
		}
		
		LOGE("XMLAddNode not find sid:%s",sidName(sid));

	}
	else {
		LOGE("sid:%d error", sid);
		return -1;
	}
	return -1;

}

int Monitor::XMLAddNode(const char * ip, short port, int sid, int nid, const char * name, int route_stragry)
{
	return 0;
}

int Monitor::XMLDelNode(int sid, int nid)
{
	TiXmlElement *root = ConfigFileReader::getInstance()->getRoot();
	TiXmlElement* services;
	if (!ConfigFileReader::getInstance()->getNodePointerByName(root, CONF_SERVICEES, services)) {
		LOGE("not find services elem");
		return -1;
	}
	const char* sidname = sidName(sid);
	if (strcmp(sidname, "null")) {
		TiXmlNode* service = NULL; //初始化
		while (service = services->IterateChildren("service", service))
		{
			const char* type = service->ToElement()->Attribute("type");
			LOGD("type:%s", type);
			if (!strcmp(type, sidname)) {
				TiXmlNode* node = NULL; //初始化
				TiXmlElement* ser = service->ToElement();
				while (node = ser->IterateChildren("node", node)) {

					for (TiXmlElement* elem = node->FirstChildElement(); elem != NULL; elem = elem->NextSiblingElement())
					{
						TiXmlNode* e1 = elem->FirstChild();
						if (!strcmp("id", elem->Value())) {
							if (nid == atoi(e1->ToText()->Value())) {
								// node->Clear();
								ser->RemoveChild(node);
								if (ConfigFileReader::getInstance()->save() == -1) {
									LOGE("save fail");
									return -1;
								}
								return 0;
							}
						}
					}
				}
				LOGE("sid:%s not find node:%d", type, nid);
				return -1;
			}
		}

		LOGE("XMLAddNode not find sid:%s", sidName(sid));
	}
	else {
		LOGE("sid:%d error", sid);
		return -1;
	}
	return -1;
}

int Monitor::XMLRouteNode(int sid, int nid, int route_stragry, router_user_list_t& user_list)
{
	TiXmlElement *root = ConfigFileReader::getInstance()->getRoot();
	TiXmlElement* services;
	if (!ConfigFileReader::getInstance()->getNodePointerByName(root, CONF_SERVICEES, services)) {
		LOGE("not find services elem");
		return -1;
	}
	const char* sidname = sidName(sid);
	if (strcmp(sidname, "null")) {
		TiXmlNode* service = NULL; //初始化
		while (service = services->IterateChildren("service", service))
		{
			const char* type = service->ToElement()->Attribute("type");
			LOGD("type:%s", type);
			if (!strcmp(type, sidname)) {
				TiXmlNode* node = NULL; //初始化
				TiXmlElement* ser = service->ToElement();
				while (node = ser->IterateChildren("node", node)) {

					TiXmlNode* item = NULL;
					item = node->IterateChildren("id", item);
					if (item) {
						if (!strcmp("id", item->Value())) {
							TiXmlNode* e1 = item->FirstChild();
							if (nid == atoi(e1->ToText()->Value())) {
								TiXmlNode* xml_stragry = NULL;
								xml_stragry = node->IterateChildren("route", xml_stragry);
								if (xml_stragry) {
									char c_stragry[16] = { 0 };
									sprintf(c_stragry, "%d", route_stragry);
									TiXmlNode* e1 = xml_stragry->FirstChild();
									xml_stragry->ToElement()->FirstAttribute()->SetValue(c_stragry);
									if (route_stragry == ROUTE_USER) {
										char users[8192] = { 0 };
										int len = 0;

										for (auto it = user_list.begin(); it != user_list.end(); it++) {
											len += snprintf(users+len, 8192 - len - 1, "%s,", it->c_str());
										}
										users[len - 1] = '\0';
										e1->ToText()->SetValue(users);
									}
									else {
										e1->ToText()->SetValue("*");
									}
									if (ConfigFileReader::getInstance()->save() == -1) {
										LOGE("save fail");
										return -1;
									}
									return 0;
								}
								else {
									TiXmlElement* xml_stragry = new TiXmlElement("route");
									char c_stragry[16] = { 0 };
									sprintf(c_stragry, "%d", route_stragry);

									xml_stragry->SetAttribute("stragry", c_stragry);

									if (route_stragry == ROUTE_USER) {
										char users[8192] = { 0 };
										int len = 0;

										for (auto it = user_list.begin(); it != user_list.end(); it++) {
											len += snprintf(users + len, 8192 - len - 1, "%s,", it->c_str());
										}
										users[len - 1] = '\0';
										xml_stragry->LinkEndChild(new TiXmlText(users));
									}
									else {
										xml_stragry->LinkEndChild(new TiXmlText("*"));
									}
									node->ToElement()->LinkEndChild(xml_stragry);
								}
								
							}
						}
					}
				}
				
				LOGE("sid:%s not find node:%d", type, nid);
				return -1;
			}
		}

		LOGE("XMLAddNode not find sid:%s", sidName(sid));
	}
	else {
		LOGE("sid:%d error", sid);
		return -1;
	}
	return -1;
}

int Monitor::XMLRouteNode(int sid, int nid, int route_stragry, const char * slots)
{
	TiXmlElement *root = ConfigFileReader::getInstance()->getRoot();
	TiXmlElement* services;
	if (!ConfigFileReader::getInstance()->getNodePointerByName(root, CONF_SERVICEES, services)) {
		LOGE("not find services elem");
		return -1;
	}
	const char* sidname = sidName(sid);
	if (strcmp(sidname, "null")) {
		TiXmlNode* service = NULL; //初始化
		while (service = services->IterateChildren("service", service))
		{
			const char* type = service->ToElement()->Attribute("type");
			LOGD("type:%s", type);
			if (!strcmp(type, sidname)) {
				TiXmlNode* node = NULL; //初始化
				TiXmlElement* ser = service->ToElement();
				while (node = ser->IterateChildren("node", node)) {

					TiXmlNode* item = NULL;
					item = node->IterateChildren("id", item);
					if (item) {
						if (!strcmp("id", item->Value())) {
							TiXmlNode* e1 = item->FirstChild();
							if (nid == atoi(e1->ToText()->Value())) {
								TiXmlNode* xml_stragry = NULL;
								xml_stragry = node->IterateChildren("route", xml_stragry);
								if (xml_stragry) {
									char c_stragry[16] = { 0 };
									sprintf(c_stragry, "%d", route_stragry);
									TiXmlNode* e1 = xml_stragry->FirstChild();
									xml_stragry->ToElement()->FirstAttribute()->SetValue(c_stragry);
									if (route_stragry == ROUTE_SLOT) {
										e1->ToText()->SetValue(slots);
									}
									else {
										e1->ToText()->SetValue("*");
									}
									if (ConfigFileReader::getInstance()->save() == -1) {
										LOGE("save fail");
										return -1;
									}
									return 0;
								}
								else {
									TiXmlElement* xml_stragry = new TiXmlElement("route");
									char c_stragry[16] = { 0 };
									sprintf(c_stragry, "%d", route_stragry);

									xml_stragry->SetAttribute("stragry", c_stragry);

									if (route_stragry == ROUTE_SLOT) {
										xml_stragry->LinkEndChild(new TiXmlText(slots));
									}
									else {
										xml_stragry->LinkEndChild(new TiXmlText("*"));
									}
									node->ToElement()->LinkEndChild(xml_stragry);
								}

							}
						}
					}
				}

				LOGE("sid:%s not find node:%d", type, nid);
				return -1;
			}
		}

		LOGE("XMLAddNode not find sid:%s", sidName(sid));
	}
	else {
		LOGE("sid:%d error", sid);
		return -1;
	}
	return -1;
}

Node::Node()
{
	enable = false;
	fd = -1;
	state = NODE_UNABLE;
	memset(ip, 0, sizeof(ip));
	memset(name, 0, sizeof(name));
	route_stragry = ROUTE_HASH;
	memset(slots, 0, sizeof(slots));
}
