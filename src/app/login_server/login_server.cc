#include "login_server.h"
#include <unistd.h>
#include<node_mgr.h>
#include <IM.Server.pb.h>
#include <IM.Basic.pb.h>
#include <IM.Login.pb.h>
#include<log_util.h>
#include <config_file_reader.h>
#include <cstdio>
#include <hash.h>
using namespace com::proto::server;
using namespace com::proto::basic;
using namespace com::proto::login;

static int MAX_USERS = 5000000;
static const int db_index_user_info = 3;
User::User(const std::string& user_id) {
	userid_ = user_id;
	device_type_ = DeviceType_UNKNOWN;
	online_status_ = USER_STATE_LOGOUT;
	version = 0;
	online_time = 0;
	nid_ = 0;
	sockfd_ = 0;
	userid_ = user_id;
}

LoginServer::LoginServer() {
	m_loop = NULL;
	m_pNode = CNode::getInstance();
}

LoginServer::~LoginServer()
{
	delete m_loop;
	auto it = m_user_map.begin();
	while (it != m_user_map.end()) {
		delete it->second;
		++it;
	}
}

LoginServer * LoginServer::getInstance()
{
	static LoginServer instance;
	return &instance;
}

int LoginServer::init()
{
    LOGD("------this:%d-----",this);
	m_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
	m_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
	if (m_ip== "" || m_port == 0) {
		LOGE("not config ip or port");
		return -1;
	}
	std::string redis_ip = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_IP);
	short redis_port = ConfigFileReader::getInstance()->ReadInt(CONF_REDIS_PORT);
	std::string auth = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_AUTH);
	LOGI("redis ip:%s,port:%d",redis_ip.c_str(),redis_port);

	redis_client.Init_Pool(redis_ip, redis_port,auth);
    
	for (int i = 0; i < LOGIN_THREAD; i++) {
		m_process[i].init(ProcessClientMsg, 1);
	}
	
	m_loop = getEventLoop();
	m_loop->init(1024);
	TcpService::init(m_loop);
	m_pNode->init(this);
	m_stateService.init(this,m_pNode);
	NodeMgr::getInstance()->init(m_loop, innerMsgCb, this);
	NodeMgr::getInstance()->setConnectionStateCb(connectionStateEvent, this);
}

void LoginServer::start() {
	for (int i = 0; i < LOGIN_THREAD; i++) {
		m_process[i].start();
	}
	sleep(1);
	LOGI("LoginServer listen on %s:%d", m_ip.c_str(), m_port);
    TcpService::StartServer(m_ip, m_port);
	PollStart();
}

extern int total_recv_pkt;
extern int total_user_login;

void LoginServer::OnRecv(int sockfd, PDUBase* base) {
	
	SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
	int cmd = base->command_id;
	switch (cmd) {
	case CID_S2S_AUTHENTICATION_REQ:
	case CID_S2S_PING:
		m_pNode->handler(sockfd, *spdu);
		break;
	default:
		LOGE("unknow cmd(%d)", cmd);
		break;
	}
	delete base;
}

void LoginServer::OnConn(int sockfd) {
    //LOGD("建立连接fd:%d", sockfd);
}

void LoginServer::OnDisconn(int sockfd) {
	m_pNode->setNodeDisconnect(sockfd);
	CloseFd(sockfd);
}

void LoginServer::ProcessClientMsg(int sockfd, SPDUBase* base)
{
	SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
	LoginServer* pInstance = LoginServer::getInstance();
	switch (spdu->command_id) {
	case HEART_BEAT:
		pInstance->ProcessHeartBeat(sockfd, *spdu);
		break;
	case USER_LOGIN:
		//++total_user_login;
		pInstance->ProcessUserLogin(sockfd, *spdu);
		//          _ProcessUserLogin(sockfd,&base);
		break;

	case USER_LOGOFF :
		pInstance->ProcessUserLogout(sockfd, *spdu,false);
		break;
	case CID_USER_CONNECT_EXCEPT:
		pInstance->ProcessUserLogout(sockfd, *spdu, true);
		break;
	default:
		LOGE("unknow cmd:%d", spdu->command_id);
		break;
	}
	delete spdu;
}

int LoginServer::processInnerMsg(int sockfd, SPDUBase & base)
{
	int cmd = base.command_id;
	char* user_id = base.terminal_token;
	int i= dictGenHashFunction((const unsigned char*)&user_id, sizeof(base.terminal_token)) % LOGIN_THREAD;
	switch (cmd) {
	case HEART_BEAT:
	case USER_LOGIN:
	case USER_LOGOFF:
	case CID_USER_CONNECT_EXCEPT:
		m_process[i].addJob(sockfd, &base);
		break;
	default:
		LOGE("unkonw cmd(%d)", cmd);
		break;
	}
}

int LoginServer::innerMsgCb(int sockfd, SPDUBase & base, void * arg)
{
	LoginServer* server = reinterpret_cast<LoginServer*>(arg);
	if (server) {
		server->processInnerMsg(sockfd, base);
	}
}

/*
* 心跳
* 回复空包
* 线程处理失败消息列表
*/
void LoginServer::ProcessHeartBeat(int sockfd, SPDUBase& _pack) {
	//    LOGDEBUG(_pack.command_id, _pack.seq_id, "心跳userid:%d,fd:%d", _pack.terminal_token, sockfd);
	std::string user_id(_pack.terminal_token,sizeof(_pack.terminal_token));
	User* user = findUser(user_id);
	if (user) {
		if (user->online_status_ == USER_STATE_OFFLINE || user->online_status_==USER_STATE_LOGOUT) {
			++m_onliners;
			user->online_status_ = USER_STATE_ONLINE;
			user->nid_ = _pack.node_id;
			user->sockfd_ = _pack.sockfd;
			saveUserState(user);
			m_stateService.publish(user);
		}
	}
	/*else {
		user = getUser(user_id);
		if (!user) {
			LOGE("too much user onliners(%d) upper max cofig onliners(%d) ,get user fail", m_onliners, MAX_USERS);
			return;
		}
	}*/
	Heart_Beat heart;
	heart.ParseFromArray(_pack.body.get(), _pack.length);

	// 回复心跳包
	Heart_Beat_Ack heart_ack;
	_pack.command_id = HEART_BEAT_ACK;
	std::shared_ptr<char> body(new char[heart_ack.ByteSize()], carray_deleter);
	heart_ack.SerializeToArray(body.get(), heart_ack.ByteSize());
	_pack.body = body;
	_pack.length = heart_ack.ByteSize();

	if (NodeMgr::getInstance()->sendNode(SID_CONN, _pack.node_id, _pack) == -1) {
		LOGE("send node[sid:%s,nid:%d]fail", sidName(SID_CONN), _pack.node_id);
	}
	
}

int LoginServer::BuildUserCacheInfo(User* user,SPDUBase& base, User_Login& _login) {
	
    std::string device_id = _login.device_id();
	int device_type = _login.has_device_type() ? _login.device_type() : DeviceType_UNKNOWN;
	int version = 0;
	if (_login.has_version()) {
		version = _login.version();
	}

	int nid = base.node_id;
	user->nid_ = nid;
	user->sockfd_ = base.sockfd;
	
	user->online_status_ = USER_STATE_ONLINE;
	user->version = version;
	
	user->device_id_ = _login.has_device_id() ? _login.device_id() : "";
	user->device_type_ = _login.has_device_type() ? _login.device_type() : DeviceType_UNKNOWN;
	
	user->online_time = time(0);

	return ERRNO_CODE_OK;
}

void LoginServer::ProcessUserLogin(int sockfd, SPDUBase& base) {
	//LOGI("===开始处理用户登录timestamp:%d===", timestamp_int());

	User_Login login;
	if (!login.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "User_Login包解析错误");
		return;
	}
	int login_result= ERRNO_CODE_OK;
	bool user_exist = false;
	do {
		
		if (!login.has_user_id()) {
			login_result = ERRNO_CODE_USER_ID_ERROR;
			break;
		}
		
		if (!login.has_device_id()) {
			login_result = ERRNO_CODE_NOT_DEVICE_ID;
			break;
		}
		
		//bool user_exist = redis_client.IsCZYUser(login.user_id());
	    user_exist = redis_client.HasUserInfo(login.user_id());
		if (!user_exist) {
			LOGE("user: %s is not czy", login.user_id().c_str());
//			login_result = ERRNO_CODE_NOT_HUXIN_USER;
			break;
		}
		
	} while (0);

	std::string device_id = login.device_id();
	int device_type = login.has_device_type() ? login.device_type() : DeviceType_UNKNOWN;

	bool relogin = false;
	const std::string& user_id = login.user_id();

	User* user = findUser(user_id);
	if (user) {
		if (user->online_status_ == USER_STATE_ONLINE) {
			relogin = true;
			if (user->device_id_ != device_id )
			{
				LOGI("kicked user[user_id:%s,%s:%d] by [%s :%d]", user_id.c_str(),user->device_id_.c_str(), user->device_type_, device_id.c_str(), device_type);
				KickedNotify(user, sockfd, device_id, device_type);
			}
		}
	}
	else {
		user = getUser(user_id);
		if (!user) {
			LOGE("too much user onliners(%d) upper max cofig onliners(%d) ,get user fail", (int)m_onliners, MAX_USERS);
			return;
		}
	}
  
	//record every connect server login user;
	{
		if (!relogin) {
			std::lock_guard<std::recursive_mutex> lock_1(m_node_userid_mutex);
			auto it = m_node_user_list.find(base.node_id);
			if (it == m_node_user_list.end()) {
				std::list<std::string> user_list;
				user_list.push_back(user_id);
				m_node_user_list[base.node_id] = user_list;
			}
			else {
				it->second.push_back(user_id);
			}
		}
	}


	if (!relogin) {
		++m_onliners;
	}
	
	BuildUserCacheInfo(user,base, login);
	saveUserState(user);
	m_stateService.publish(user);
	//m_stateService.notify(user->userid_, true, user->online_time);
	//int login_result=ERRNO_CODE_OK;
	
	
	//LOGD("回复登录结果 %d", login_result);
	User_Login_Ack login_ack;
	login_ack.set_errer_no((ERRNO_CODE)login_result);
	login_ack.set_upload(!user_exist);
	base.ResetPackBody(login_ack, USER_LOGIN_ACK);
	
//	NodeMgr::getInstance()->sendsock(SID_DISPATCH,sockfd, base);
	if (NodeMgr::getInstance()->sendNode(SID_CONN, base.node_id, base) == -1) {
		LOGE("send node[sid:%s,nid:%d]fail", sidName(SID_CONN), base.node_id);
	}
	
	LOGD("---------->user [userid:%s, fd:%d] login(%d) version(%d)------loginers[%d]--user_map[%d]------->", user_id.c_str(),  sockfd, login_result,user->version, (int)m_onliners, m_user_map.size());
	//LOGI("~~~结束处理用户登录timestamp:%d~~~", timestamp_int());
}

void LoginServer::ProcessUserLogout(int sockfd, SPDUBase & base,bool except)
{
	User_LogOff logout;
	if (!logout.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "user logout pkt parse fail");
		return;
	}
	
	const std::string& user_id = logout.user_id();

	User* user = findUser(user_id);
	if (!user) {
        LOGE("not find user_id(%s)", user_id.c_str());
		return;
	}
    if(base.sockfd!=user->sockfd_){
        return;
    }
	if (user->online_status_ == USER_STATE_ONLINE) {
		--m_onliners;
	}
	LOGD("user[userid:%s ] %s", user_id.c_str(), except? "disconnect":"logout");
	if (except) {
		user->online_status_ = USER_STATE_OFFLINE;
	}
	else {
		user->online_status_ = USER_STATE_LOGOUT;
	}
	deleteUserState(user_id);
	m_stateService.publish(user);
}

void LoginServer::KickedNotify(User* user,int sockfd, std::string device_id, int device_type)
{
	Multi_Device_Kicked_Notify notify;
	notify.set_timestamp(time(NULL));
	notify.set_user_id(user->userid_);
	notify.set_new_device((Device_Type)device_type);
	notify.set_new_device_id(device_id);
	SPDUBase spdu;
	spdu.node_id = user->nid_;
	spdu.sockfd = user->sockfd_;
	memcpy(spdu.terminal_token ,user->userid_.c_str(),sizeof(spdu.terminal_token));
	spdu.ResetPackBody( notify, MULTI_DEVICE_KICKED_NOTIFY);
//	NodeMgr::getInstance()->sendsock(SID_DISPATCH, sockfd, spdu);
	if (NodeMgr::getInstance()->sendNode(SID_CONN, spdu.node_id, spdu) == -1) {
		LOGE("send node[sid:%s,nid:%d]fail", sidName(SID_CONN), spdu.node_id);
	}

}

void LoginServer::Timer(int fd, short mask, void * privdata)
{
	LoginServer* server = reinterpret_cast<LoginServer*>(privdata);
	if (!server) {
		return;
	}

}

void LoginServer::connectionStateEvent(NodeConnectedState* state,void* arg)
{
	if (state->state != SOCK_CONNECTED) {
		return;
	}
	
	LoginServer* server = reinterpret_cast<LoginServer*>(arg);
	if (server) {
		if (state->sid == SID_DISPATCH) {
			server->registCmd(state->sid, state->nid);
		}
		
	}
}

void LoginServer::registCmd(int sid,int nid)
{
    LOGD("registcmd %s",sidName(sid));
	RegisterCmdReq req;
	int cur_nid= ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
	int cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
	req.set_node_id(cur_nid);
	req.set_service_type(cur_sid);
	req.add_cmd_list(USER_LOGIN);
	req.add_cmd_list(USER_LOGOFF);
	req.add_cmd_list(CID_USER_CONNECT_EXCEPT);
	req.add_cmd_list(HEART_BEAT);
	SPDUBase spdu;
	spdu.ResetPackBody(req, CID_REGISTER_CMD_REQ);
	NodeMgr::getInstance()->sendNode(sid, nid,spdu);
}

int LoginServer::saveUserState(User * user)
{
	//state format:  nid:%d_fd:%d_dt:%d_v:%d_ph:%s
	char state[128] = { 0 };
	sprintf(state, "nid:%d_fd:%d_dt:%d_v:%d", user->nid_, user->sockfd_,user->device_type_, user->version);
	LOGD("sess:%s", state);
	if (!redis_client.setUserState(user->userid_, state)) {
		LOGE("save user[user_id:%s] state fail", user->userid_.c_str());
	}
}

int LoginServer::deleteUserState(const std::string&  user_id) {
	redis_client.deleteUserState(user_id);
}

#if 0
void LoginServer::addConnection(int sockfd, int sid, int nid)
{
	SNode* node = new SNode;
	node->fd = sockfd;
	node->sid = sid;
	node->nid = nid;
	m_nodes.push_back(node);
	m_stateService.subscribe(sockfd);
}

void LoginServer::delConnection(int sockfd)
{
	auto it = m_nodes.begin();
	while (it != m_nodes.end()) {
		if ((*it)->fd == sockfd) {
			delete *it;
			m_nodes.erase(it);
			break;
		}
		++it;
	}
	m_stateService.unsubscribe(sockfd);
}
#endif
User * LoginServer::getUser(const std::string&  user_id)
{
	User* user = NULL;
	if (m_onliners >= MAX_USERS-1024) {
		LOGW("loginers(%d) upper max loginer user(%d),get user fail", (int)m_onliners, MAX_USERS);
		return NULL;
	}
	std::lock_guard<std::recursive_mutex> lock_1(m_user_map_mutex);
	if (m_user_map.size() >= MAX_USERS) {
		if (MAX_USERS > m_onliners && m_onliners > MAX_USERS*0.8) {
			LOGW("too much loginers(%d)", (int)m_onliners);
		}
		auto rit = m_user_map.rbegin();
		for (rit; rit != m_user_map.rend(); rit++) {
			if (rit->second->online_status_ == USER_STATE_LOGOUT) {
				user = rit->second;
				user->clear();
				m_user_map.erase(rit);
				break;
			}
		}
		if (!user) {
			LOGE("onliners(%d) too much,alloc user fail", (int)m_onliners);
			return NULL;
		}
	}
	else {
		user = new User(user_id);
	}
	user->userid_ = user_id;
	m_user_map.push(user_id, user);
	return user;
}

User * LoginServer::findUser(const std::string&  user_id)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_user_map_mutex);
	auto it = m_user_map.find(user_id);
	if (it != m_user_map.end()) {
		return it->second;
	}
	return NULL;
}

