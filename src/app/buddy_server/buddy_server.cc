#include "buddy_server.h"
#include "log_util.h"
#include "config_file_reader.h"
#include "IM.Msg.pb.h"
#include "time_util.h"
#include "logic_util.h"
#include <unistd.h>
#include <node_mgr.h>
#include <string.h>
#include "IM.State.pb.h"
#include <node_mgr.h>
#include <IM.Server.pb.h>

using namespace com::proto::msg;
using namespace com::proto::state;
using namespace com::proto::server;

static int sec_recv_pkt;
static int total_recv_pkt;

static int sec_send_pkt;
int total_send_pkt;
namespace buddy{
	static  const int db_index_im_msg = 2;
	static const int db_index_offline_msg = 15;
	static const int db_index_user_info = 3;
BuddyServer::BuddyServer() :loop_(getEventLoop()), tcpService_(this, loop_) {
    m_cur_index_ = 0;
    m_node_id_=0;
    m_lastTime = 0;
    m_pNode = CNode::getInstance();
	m_dbproxy_fd = -1;
}

int BuddyServer::sendConn(SPDUBase& base)
{
	if (NodeMgr::getInstance()->sendNode(SID_CONN, base.node_id, base) == -1) {
		LOGE("send node[sid:%s,nid:%d]fail", sidName(SID_CONN), base.node_id);
	}
}

BuddyServer * BuddyServer::getInstance()
{
    static BuddyServer instance;
    return &instance;
}

int BuddyServer::init()
{
    m_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
    m_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
    if (m_ip == "" || m_port == 0) {
        LOGE("not config ip or port");
        return -1;
    }
    std::string redis_ip = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_IP);
    short redis_port = ConfigFileReader::getInstance()->ReadInt(CONF_REDIS_PORT);
    std::string auth = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_AUTH);
    LOGD("redis ip:%s,port:%d", redis_ip.c_str(), redis_port);
   
    m_buddy_cache.init();

	if (storage_offline_msg_redis_.connect(redis_ip, redis_port, auth, db_index_offline_msg) != 0) {
		LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
		return -1;
	}
    std::string msg_queue_ip = ConfigFileReader::getInstance()->ReadString(CONF_MSG_QUEUE_IP);
    short  msg_queue_port = ConfigFileReader::getInstance()->ReadInt(CONF_MSG_QUEUE_PORT);
    std::string msg_queue_auth = ConfigFileReader::getInstance()->ReadString(CONF_MSG_QUEUE_AUTH);
	if (storage_msg_redis_.connect(msg_queue_ip, msg_queue_port, msg_queue_auth, db_index_im_msg) != 0) {
		LOGE("redis: connect redis ip:%s,port:%d fail", msg_queue_ip.c_str(), msg_queue_port);
		return -1;
	}

	if (user_info_.connect(redis_ip, redis_port, auth, db_index_user_info) != 0) {
		LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
		return -1;
	}
    //m_common_msg_process.init(ProcessClientMsg, 2);

    m_storage_msg_works = new ThreadPool(1);//save msg redis;
    m_chat_msg_process.init(ProcessClientMsg,2);

	std::string dbproxy_ip = ConfigFileReader::getInstance()->ReadString(CONF_DBPROXY_IP);
	short dbproxy_port = ConfigFileReader::getInstance()->ReadInt(CONF_DBPROXY_PORT);
	m_dbproxy_fd = tcpService_.connect(dbproxy_ip, dbproxy_port);
	if (m_dbproxy_fd == -1) {
		LOGE("connect dbproxy(addr: %s:%d) fail",dbproxy_ip.c_str(),dbproxy_port);
//		return -1;
	}
    //CreateTimer(1000, Timer, this);
    m_pNode->init(&tcpService_);
    NodeMgr::getInstance()->init( loop_, innerMsgCb, this);
    NodeMgr::getInstance()->setConnectionStateCb(connectionStateEvent, this);
	loop_->createTimeEvent(1000, Timer, this);
}

void BuddyServer::Timer(int fd, short mask, void * privdata)
{
	BuddyServer* pInstance = reinterpret_cast<BuddyServer*>(privdata);
	if (pInstance) {
		pInstance->cron();
	}
}
void BuddyServer::cron()
{
	if (m_dbproxy_fd == -1) {
		std::string dbproxy_ip = ConfigFileReader::getInstance()->ReadString(CONF_DBPROXY_IP);
		short dbproxy_port = ConfigFileReader::getInstance()->ReadInt(CONF_DBPROXY_PORT);
        if(dbproxy_ip!=""){
		m_dbproxy_fd = tcpService_.connect(dbproxy_ip, dbproxy_port);
		if (m_dbproxy_fd == -1) {
			LOGE("connect dbproxy(addr: %s:%d) fail", dbproxy_ip.c_str(), dbproxy_port);
		}
	}
    }

	std::lock_guard<std::recursive_mutex> lock_1(m_buddylistlock);
	//处理缓存消息
	map<std::string, list<MHandler*> > ::iterator it = m_msgHandler.begin();
	for (it; it != m_msgHandler.end();) {

		list<MHandler*>& li = it->second;
		list<MHandler*>::iterator lit = li.begin();
		for (lit; lit != li.end(); ) {
			MHandler* pHandler = *lit;
			TIME_T now;
			getCurTime(&now);
			if (interval(pHandler->time, now) > 15 * 1000) {
				LOGE("long time handler user(%s) member req,will drop it", it->first.c_str());
				//buffree(pHandler->msg);
				delete pHandler;
				li.erase(lit++);
			}
			else {
				lit++;
			}
		}
		if (li.empty()) {
			m_msgHandler.erase(it++);
		}
		else {
			++it;
		}
	}
}
void BuddyServer::start() {
    m_chat_msg_process.start();
    sleep(1);
    LOGD("BuddyServer listen on %s:%d", m_ip.c_str(), m_port);
	tcpService_.listen(m_ip, m_port);
	tcpService_.run();
}

extern int total_recv_pkt;
extern int total_user_login;

void BuddyServer::connectionStateEvent(NodeConnectedState* state, void* arg)
{
    BuddyServer* server = reinterpret_cast<BuddyServer*>(arg);
    if (!server) {
        return;
    }
    if (state->sid == SID_DISPATCH && state->state == SOCK_CONNECTED) {
        server->registCmd(state->sid, state->nid);
    }
}

void BuddyServer::registCmd(int sid, int nid)
{
    LOGD("registcmd %s", sidName(sid));
    RegisterCmdReq req;
    m_node_id_ = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
    int cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
    req.set_node_id(m_node_id_);
    req.set_service_type(cur_sid);
    req.add_cmd_list(CID_CHAT_BUDDY);
    req.add_cmd_list(CID_CHAT_MACHINE);
    req.add_cmd_list(CID_CHAT_CUSTOMER_SERVICES);
    req.add_cmd_list(CID_BUDDY_LIST_OPT_REQ);
	req.add_cmd_list(CID_BUDDY_LIST_REQ);
	req.add_cmd_list(CID_BUDDY_LIST_RSP);
	req.add_cmd_list(CID_ORG_LIST_REQ);
	req.add_cmd_list(CID_USER_INFO_REQ);
	req.add_cmd_list(CID_USER_INFO_OPT_REQ);
	
    SPDUBase spdu;
    spdu.ResetPackBody(req, CID_REGISTER_CMD_REQ);
    NodeMgr::getInstance()->sendNode(sid, nid, spdu);
}

void BuddyServer::onData(int sockfd, PDUBase* base) {
    SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
    int cmd = spdu->command_id;

    switch (cmd) {
        case CID_ORG_LIST_RSP:
		case CID_BUDDY_LIST_OPT_RSP:
        case CID_BUDDY_LIST_RSP:
		case CID_USER_INFO_RSP:
		case CID_S2S_BUDDY_LIST_RSP:
            m_chat_msg_process.addJob(sockfd, spdu);
            break;
        case CID_S2S_AUTHENTICATION_REQ:
        case CID_S2S_PING:
            m_pNode->handler(sockfd, *spdu);
            delete base;
            break;
        default:
            LOGE("unknow cmd(%d)", cmd);
            delete base;
            break;
    }
}

void BuddyServer::onEvent(int fd, ConnectionEvent event)
{
	if (event ==Disconnected ) {
		if (fd == m_dbproxy_fd) {
			m_dbproxy_fd = -1;
		}
		m_pNode->setNodeDisconnect(fd);
		tcpService_.closeSocket(fd);
	}
}

/* ................handler msg recving from dispatch.............*/
int BuddyServer::processInnerMsg(int sockfd, SPDUBase & base)
{
    int cmd = base.command_id;
    int index = 0;
    switch (cmd) {
        //user msg
        case CID_BUDDY_LIST_REQ:
        case CID_BUDDY_LIST_RSP:
        case CID_BUDDY_LIST_OPT_REQ://state msg
        case CID_CHAT_CUSTOMER_SERVICES:
        case CID_CHAT_MACHINE:
		case CID_CHAT_BUDDY:
		case CID_ORG_LIST_REQ:
		case CID_USER_INFO_REQ:
		case CID_USER_INFO_OPT_REQ:
	
	//	case LOCATIONSHARE_INVIT:
	//	case LOCATIONSHARE_JOIN:
	//	case LOCATIONSHARE_QUIT:
            m_chat_msg_process.addJob(sockfd, &base);
            break;
        default:
            LOGE("unknow cmd(%d)", cmd);
            delete &base;
            break;
    }

    //printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~<<<<<出\n");
    return 0;
}

int BuddyServer::innerMsgCb(int sockfd, SPDUBase & base, void * arg)
{
    BuddyServer* server = reinterpret_cast<BuddyServer*>(arg);
    if (server) {
        server->processInnerMsg(sockfd, base);
    }
}

void BuddyServer::ProcessClientMsg(int sockfd, SPDUBase* base)
{
    BuddyServer* pInstance = BuddyServer::getInstance();
    //check user whether onlines;
	std::string user_id(base->terminal_token, sizeof(base->terminal_token));
    switch (base->command_id) {
        case CID_BUDDY_LIST_REQ:
            pInstance->buddyReqListReq(sockfd, *base);
            break;
        case CID_BUDDY_LIST_RSP:
			
            pInstance->buddyReqListRsp(sockfd, *base);
            break;
        case CID_BUDDY_LIST_OPT_REQ:
           
            pInstance->optBuddyReq(sockfd, *base);
            break;
      
        case CID_BUDDY_LIST_OPT_RSP:
			pInstance->optBuddyRsp(sockfd, *base);
            break;
        case CID_CHAT_CUSTOMER_SERVICES:
        case CID_CHAT_MACHINE:
		case CID_CHAT_BUDDY:
			pInstance->ProcessIMChat_Personal(sockfd, *base);
			break;
		case CID_ORG_LIST_REQ:
			pInstance->orgListReq(sockfd, *base);
			break;

		case CID_ORG_LIST_RSP:
			pInstance->orgListRsp(sockfd, *base);
			break;
		case CID_USER_INFO_REQ:
			pInstance->userInfoReq(sockfd, *base);
			break;
		case CID_USER_INFO_RSP:
			pInstance->userInfoRsp(sockfd, *base);
			break;
		case CID_USER_INFO_OPT_REQ:
			pInstance->userInfoOptReq(sockfd, *base);
			break;
		case CID_S2S_BUDDY_LIST_RSP:
			pInstance->buddyListRsp(sockfd, *base);
			break;
        default:
            LOGE("unknow cmd:%d", base->command_id);
            break;
    }
    delete base;

}

void BuddyServer::orgListReq(int sockfd, SPDUBase&  base) {
	IMGetOrgReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "IMGetOrgReq parse fail");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
    LOGD("orgListReq orgid:%s",req.org_id().c_str());
	
	tcpService_.Send(m_dbproxy_fd, base);
}

void BuddyServer::orgListRsp(int sockfd, SPDUBase&  base) {
	IMGetOrgRsp rsp;
	if (!rsp.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "IMGetOrgRsp parse fail");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
	
    LOGD("orgListRsp orgid:%s",rsp.org_id().c_str());
	sendConn(base);
}

bool BuddyServer::getUserInfo(const std::string& user_id, UserInfo& user_ifno) {
	std::string state;
	const char *command[2];
	size_t vlen[2];

	std::string key = "ui_";
	key += user_id;

	command[0] = "get";
	command[1] = key.c_str();

	vlen[0] = 3;
	vlen[1] = key.length();
	std::string result;

	bool res = user_info_.query(command, sizeof(command) / sizeof(command[0]), vlen, result);
	if (res) {
		if (!user_ifno.ParseFromArray(result.data(), result.length())) {
			LOGE("BuddyList parse fail");
			return false;
		}
		return true;
	}
	return false;
}

bool BuddyServer::delUserInfo(const std::string& user_id) {
	std::string state;
	const char *command[2];
	size_t vlen[2];

	std::string key = "ui_";
	key += user_id;
	command[0] = "del";
	command[1] = key.c_str();
	
	vlen[0] = 3;
	vlen[1] = key.length();
	std::string result;

	bool res = user_info_.query(command, sizeof(command) / sizeof(command[0]), vlen, result);
	if (!res) {
		LOGE("delete user(%s) user info fail", user_id.c_str());
		return false;
	}
	return true;
}

bool BuddyServer::setUserInfo(const std::string& user_id,const void* data,int len) {
	std::string state;
	const char *command[3];
	size_t vlen[3];

	std::string key = "ui_";
	key += user_id;
	command[0] = "set";
	command[1] = key.c_str();
	command[2] = (const char*)data;
	vlen[0] = 3;
	vlen[1] = key.length();
	vlen[2] = len;
	std::string result;

	bool res = user_info_.query(command, sizeof(command) / sizeof(command[0]), vlen, result);
	if (!res) {
		LOGE("set user(%s) user info fail", user_id.c_str());
		return false;
	}
	return true;
}
void BuddyServer::userInfoReq(int sockfd, SPDUBase&  base) {
	IMGetUserInfoReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "IMGetUserInfoReq parse fail");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
	IMGetUserInfoRsp rsp;
	IMGetUserInfoReq request;
	request.set_type(req.type());
	request.set_user_id(req.user_id());
	auto user_ids_list = rsp.user_info_list();
	const auto& user_items = req.user_item_list();
	long long start = get_mstime();
	for (int i = 0; i < user_items.size(); i++) {
		UserInfo user_info;
		if (getUserInfo(user_items[i], user_info)) {
			auto pItem = rsp.add_user_info_list();
			pItem->CopyFrom(user_info);
		}
		else {
			request.add_user_item_list(user_items[i]);
		}
	}
	long long end = get_mstime();
	LOGD(" total(%d) get(num:%d) users_info %d ms ", req.user_item_list_size(), rsp.user_info_list_size(), end - start);
	if (request.user_item_list_size() > 0) {
		LOGD("query (num:%d) users_info from ice", request.user_item_list_size());
		ResetPackBody(base, request, CID_USER_INFO_REQ);
		tcpService_.Send(m_dbproxy_fd, base);
	}
	if (rsp.user_info_list_size() > 0) {
		ResetPackBody(base, rsp, CID_USER_INFO_RSP);
		sendConn(base);
	}
}

void BuddyServer::userInfoRsp(int sockfd, SPDUBase&  base) {
	IMGetUserInfoRsp rsp;
	if (!rsp.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "IMGetUserInfoRsp parse fail");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
	sendConn(base);
}

void BuddyServer::userInfoOptReq(int sockfd, SPDUBase&  base) {
	IMInfoOptReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "userInfoOptReq parse fail");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
	int type = req.type();
	const std::string& user_id = req.user_id();
	LOGD("userInfoOptReq user(%s) opt type(%d)", user_id.c_str(), type);
	if (type == 0) {//delete userinfo
		delUserInfo(user_id);
	}
	if (type == 1) {//add userinfo
		const auto& user_ifo = req.user_info();
		std::shared_ptr<char> body(new char[user_ifo.ByteSize()], carray_deleter);
		user_ifo.SerializeToArray(body.get(), user_ifo.ByteSize());
		setUserInfo(user_id, body.get(), user_ifo.ByteSize());
	}
	IMInfoOptRsp rsp;
	rsp.set_result(RESULT_CODE_SUCCESS);
	rsp.set_type(type);
	ResetPackBody(base, rsp, CID_USER_INFO_OPT_RSP);
	sendConn(base);
}

void BuddyServer::buddyReqListReq(int sockfd, SPDUBase&  base) {
	IMGetBuddyReqListReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "IMGetBuddyReqListReq 包解析错误");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
	IMGetBuddyReqListRsp rsp;
	const std::string& user_id = req.user_id();
	uint64_t update_time = req.update_time();
	LOGD("user[%s ]  buddyReqListReq", user_id.c_str());
	user_t* user = m_buddy_cache.findUser(user_id);
	if (!user) {
		user=m_buddy_cache.getUser(user_id);
	}
	if (update_time != 0 && user->update_time == update_time) {
		LOGD("time:%lu,user[%s]  buddylist is latest", update_time, user_id.c_str());
		rsp.set_update_time(update_time);
	}
	else {
		RDBuddyList buddylist;
		if (m_buddy_cache.getBuddyList(user, buddylist)) {
			const auto& buddy_info_list = buddylist.buddy_info_list();
			auto pBuddyList = rsp.mutable_buddyinfos();
			pBuddyList->CopyFrom(buddy_info_list);
			rsp.set_update_time(buddylist.update_time());
		}
		else {
			LOGE("get user[%s] buddylist fail,send db get buddylist", user_id.c_str());
			tcpService_.Send(m_dbproxy_fd, base);
			return;
		}
	}
		
	rsp.set_result(RESULT_CODE_SUCCESS);
	ResetPackBody(base, rsp, CID_BUDDY_LIST_RSP);
	sendConn(base);
}

void BuddyServer::buddyReqListRsp(int sockfd, SPDUBase&  base) {
	IMGetBuddyReqListRsp rsp;
	if (!rsp.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "IMGetBuddyReqListRsp 包解析错误");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
	LOGD("buddyReqListRsp");
	std::string user_id = std::string(base.terminal_token,sizeof(base.terminal_token));
	uint64_t update_time = rsp.update_time();

	user_t* user = m_buddy_cache.findUser(user_id);
	if (user) {
		user->update_time = update_time;
	}
	sendConn(base);
}


void  BuddyServer::buddyListReq(const std::string& user_id, int sockfd, SPDUBase& spdu, handlerCb cb)
{
	std::lock_guard<std::recursive_mutex> lock_1(m_buddylistlock);

	MHandler* pHandler = new MHandler;

	//memset(&pHandler->msg, 0, sizeof(pHandler->msg));
	//	deep_copy(pHandler->msg, msg);
	pHandler->spdu = spdu;
	pHandler->fd = sockfd;
	pHandler->handler = cb;
	getCurTime(&pHandler->time);
	{
		map<std::string, list<MHandler*> > ::iterator it = m_msgHandler.find(user_id);
		if (it != m_msgHandler.end()) {
			it->second.push_back(pHandler);
		}
		else {
			list<MHandler*> li;
			li.push_back(pHandler);
			m_msgHandler[user_id] = li;
		}
	}
	
	SPDUBase request;
	memcpy(request.terminal_token, user_id.c_str(), sizeof(request.terminal_token));
	IMGetBuddyReqListReq req;
	req.set_user_id(user_id);
	ResetPackBody(request, req, CID_S2S_BUDDY_LIST_REQ);
	tcpService_.Send(m_dbproxy_fd, request);
}

bool BuddyServer::buddyListRsp(int sockfd, SPDUBase&  base)
{
	LOGD("buddyListRsp");
	IMGetBuddyReqListRsp rsp;
	if (!rsp.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "GroupListRsp parse fail");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return false;
	}
	
	uint64_t update_time = rsp.update_time();
	int result = rsp.result();
	if (result != RESULT_CODE_SUCCESS) {
		return false;
	}
	std::string user_id = std::string(base.terminal_token, sizeof(base.terminal_token));
	const auto& buddy_list = rsp.buddyinfos();
	user_t* user = m_buddy_cache.findUser(user_id);
	if (!user) {
		user = m_buddy_cache.getUser(user_id);
	}

	user->clearBuddy();
	auto it = buddy_list.begin();
	user->update_time=update_time;

	for (it; it != buddy_list.end(); it++) {
		user->addBuddy(it->user_id(), it->status());
	}
	std::lock_guard<std::recursive_mutex> lock_1(m_buddylistlock);
	handlerMsg(user_id);
	return true;
}

int BuddyServer::handlerMsg(const std::string& user_id)
{
	LOGD("handler user_id(%s) msg", user_id.c_str());
	//std::lock_guard<std::recursive_mutex> lock_1(m_buddylistlock);
	map<std::string, list<MHandler*> > ::iterator it = m_msgHandler.find(user_id);
	if (it != m_msgHandler.end()) {
		list<MHandler*>::iterator lit = it->second.begin();
		for (lit; lit != it->second.end();) {
			MHandler* pHandler = *lit;
			lit = it->second.erase(lit);
			handlerCb cb = pHandler->handler;
			(BuddyServer::getInstance()->*cb)(pHandler->fd, pHandler->spdu);
			delete pHandler;
		}
		m_msgHandler.erase(it);
	}
	return 0;
}
void BuddyServer::optBuddyReq(int sockfd, SPDUBase&  base) {
	IMOptBuddyReq req;
	if (!req.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "IMOptBuddyReq parse fail");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
    int type=req.opt_type();
	const std::string& src_user_id = req.src_user_id();
	const std::string& dest_user_id = req.dest_user_id();
    LOGD("src_user_id(%s),dst_user_id(%s) optBuddyReq type(%d)",src_user_id.c_str(),dest_user_id.c_str(), type);
	tcpService_.Send(m_dbproxy_fd, base);
}

void BuddyServer::optBuddyRsp(int sockfd, SPDUBase&  base) {
	IMOptBuddyRsp rsp;
	if (!rsp.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "optBuddyRsp parse fail");
		ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
		return;
	}
	int type = rsp.opt_type();
	sendConn(base);
	LOGD("optBuddyRsp");
	const std::string& dest_user_id = rsp.dest_user_id();
	int result = rsp.result();
	if (type == BUDDY_OPT_ADD_REQ || type==BUDDY_OPT_ADD_AGREE) {
		//if user A send buddy request to user B and they not buddy,we send the request to B
		if (result == CODE_OK) {
			
			uint64_t msg_id = getMsgId();
			InsertMsgIdtoRedis(dest_user_id, std::to_string(msg_id));
			int nid = NodeMgr::getInstance()->getUserNodeId(SID_MSG, dest_user_id);
			IMOptBuddyNotify notify;
			notify.set_src_user_id(rsp.src_user_id());
			notify.set_dest_user_id(rsp.dest_user_id());
			notify.set_opt_type(rsp.opt_type());
			notify.set_opt_remark(rsp.opt_remark());
			notify.set_username(rsp.username());
			notify.set_nickname(rsp.nickname());
			notify.set_avatar(rsp.avatar());
			notify.set_orgname(rsp.orgname());
			notify.set_realname(rsp.realname());
			ResetPackBody(base, notify, CID_PUSH_MSG);
			S2SPushMsg push;
			PushMsg msg;
			msg.set_data(base.body.get(), base.length);
			push.add_user_id(dest_user_id);
			msg.set_msg_id(msg_id);
			msg.set_cmd_id(CID_BUDDY_LIST_OPT_NOTIFY);
			push.set_allocated_msg(&msg);
			ResetPackBody(base, push, CID_PUSH_MSG);

			NodeMgr::getInstance()->sendNode(SID_MSG, nid, base);
			push.release_msg();
	      
		}
	}
	user_t* user = m_buddy_cache.findUser(rsp.src_user_id());
	if (user) {
		user->clearBuddy();
	}

	user = m_buddy_cache.findUser(dest_user_id);
	if (user) {
		user->clearBuddy();
	}
}

/*
 * 处理用户发送的IM消息
 * 注意：用户发送消息时只有自己的userid和对方的手机号，所以其他信息需要查询获得
 */
void BuddyServer::ProcessIMChat_Personal(int sockfd, SPDUBase&  base) {
    ChatMsg im;
    Device_Type device_type = DeviceType_UNKNOWN;

    if (!im.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "IMChat_Personal包解析错误");
        ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
        return;
    }
    int cmd=base.command_id;
	 MsgData*  data = im.mutable_data();
	const std::string& src_user_id = data->src_user_id();
	const std::string& dest_user_id = data->dest_user_id();
	int sess_type = data->session_type();
	//checkout dest user relation with src_user
	if (sess_type == SESSION_TYPE_BUDDY && src_user_id != dest_user_id) {
		user_t* user = m_buddy_cache.findUser(dest_user_id);
		if (!user) {
			user = m_buddy_cache.getUser(dest_user_id);
			if (!m_buddy_cache.getBuddyList(user)) {
				buddyListReq(dest_user_id, sockfd, base, &BuddyServer::ProcessIMChat_Personal);
				return;
			}
		}

		if (user->buddy_list.empty()) {
			if (!m_buddy_cache.getBuddyList(user)) {
				buddyListReq(dest_user_id, sockfd, base, &BuddyServer::ProcessIMChat_Personal);
				return;
			}
		}
		int status = user->buddyRelation(src_user_id);
		ERRNO_CODE code = ERRNO_CODE_OK;
		if (status == status_delete) {
			code = ERRNO_CODE_USER_IS_DELETE;
		}
		else if (status == status_black) {
			code = ERRNO_CODE_USER_IS_BLACK;
		}
		else if (status == -1) {
			code = ERRNO_CODE_USER_NOT_BUDDY;
		}
		if (code != ERRNO_CODE_OK) {
			LOGD("src_user(%s)------status(%d)----- dest_user(%s) ", src_user_id.c_str(), code,dest_user_id.c_str());
			ReplyChatResult(sockfd, base, code, 0, 0);
			return;
		}
	
	}
    // 以服务器收到消息的时间为准
    data->set_create_time(timestamp_int());
    uint64_t msg_id = getMsgId();
    LOGI("IM type:%d，from:%s to:%s (msg_id:%ld)", data->content_type(), src_user_id.c_str(), dest_user_id.c_str(), msg_id);
    if (data->content_type() == 0) {
        LOGD("content msg:%s", data->msg_content().c_str());
    }
	bool is_target_online = 0;

	ReplyChatResult(sockfd, base, ERRNO_CODE_OK, is_target_online, msg_id);

	data->set_msg_id(msg_id);
	ResetPackBody(base, im, cmd);
     

	std::string* msg_data=new std::string(base.body.get(), base.length);
   
    m_storage_msg_works->enqueue(&BuddyServer::storage_common_msg, this, msg_data);


    //LOGDEBUG(base.command_id, base.seq_id, "推送%d", device_type);
    if (device_type == DeviceType_IPhone || device_type == DeviceType_IPad) {
  
    //    m_storage_msg_works->enqueue(&BuddyServer::storage_apple_msg, this, msg_data);
    }

	InsertMsgIdtoRedis(dest_user_id, std::to_string(msg_id));
	int nid = NodeMgr::getInstance()->getUserNodeId(SID_MSG, dest_user_id);
	int cur_nid = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
	if (cur_nid != nid) {
		S2SPushMsg push;
		PushMsg msg;
		msg.set_data(base.body.get(), base.length);
		push.add_user_id(dest_user_id);
		msg.set_msg_id(msg_id);
		msg.set_cmd_id(cmd);
		push.set_allocated_msg(&msg);
		ResetPackBody(base, push, CID_PUSH_MSG);
		
        NodeMgr::getInstance()->sendNode(SID_MSG,nid,base);
		push.release_msg();
	}
}

/* ....................locationshare.............................*/
#if 0
void BuddyServer::notifyUsers(int user_id, const std::string& ph,const google::protobuf::Message &_msg,int cmd) {
	SPDUBase spdu;
	std::shared_ptr<char> body(new char[_msg.ByteSize()], carray_deleter);
	_msg.SerializeToArray(body.get(), _msg.ByteSize());
	spdu.body = body;
	spdu.length = _msg.ByteSize();
	spdu.seq_id = 0;
	spdu.command_id = cmd;
	user_t* user = m_user_cache.findUser(user_id);
	if (user) {
		if (user->online) {
			spdu.terminal_token = user->user_id;
			spdu.node_id = user->nid;
			spdu.sockfd = user->fd;
			sendConn(spdu);
		}
	}
}

void BuddyServer::ProcessLocationShareInvite(int sockfd, SPDUBase & base)
{
	LocationShare_Invit invite;
	if (!invite.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "invite parse fail");
		return;
	}
	uint64_t msg_id = getMsgId();
	invite.set_task_id(msg_id);
	int invitor_user_id = invite.invitor_user_id();
	LocationShareTask* task = new LocationShareTask(msg_id, invitor_user_id);
	int user_id = 0;
	int errno_code;
	const auto& ph_list = invite.phones();
	for (int i = 0; i < ph_list.size(); i++) {
		const std::string& ph = ph_list[i];
		errno_code = PhoneQueryUserId(ph, user_id);
		if (errno_code == ERRNO_CODE_OK && user_id != 0) {
			task->addInviteUser(user_id, ph);
		}
	}
	m_localshare[msg_id] = task;

	LocationShare_Invit_Ack ack;
	ack.set_errer_no(ERRNO_CODE_OK);
	ack.set_task_id(msg_id);
	base.ResetPackBody(ack, LOCATIONSHARE_INVIT_ACK);
	sendConn(base);

	LocationShare_Invit_Notify notify;
	notify.set_allocated_invit(&invite);
	notify.set_timestamp(timestamp_int());
	task->notifyUsers(this, &BuddyServer::notifyUsers, notify, LOCATIONSHARE_INVIT_NOTIFY);
	notify.release_invit();
}

void BuddyServer::ProcessLocationShareJoin(int sockfd, SPDUBase & base)
{
	LocationShare_Join join;
	if (!join.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "join parse fail");
		return;
	}
	uint64_t id = join.task_id();
	int user_id = join.user_id();
	auto it = m_localshare.find(id);
	LocationShareTask* task;
	if (it == m_localshare.end()) {
		return;
	}
	task = it->second;
	int status = join.answer_status();
	if (status == LocationShareAccept) {
		task->addAnswerUser(join.user_id());
	}
	LocationShare_Join_Ack ack;
	ack.set_errer_no(ERRNO_CODE_OK);
	ack.set_task_id(id);
	base.ResetPackBody(ack, LOCATIONSHARE_JOIN_ACK);
	sendConn(base);

	LocationShare_Join_Notify notify;
	notify.set_allocated_share_join(&join);
	base.ResetPackBody(notify, LOCATIONSHARE_JOIN_NOTIFY);
	base.seq_id = 0;
	
	user_id = task->invitor();
	user_t* user = m_user_cache.findUser(user_id);
	if (user) {
		if (user->online) {
			base.terminal_token = user->user_id;
			base.node_id = user->nid;
			base.sockfd = user->fd;
			sendConn(base);
		}
	}
	notify.release_share_join();

}

void BuddyServer::ProcessLocationShareQuit(int sockfd, SPDUBase & base)
{
	LocationShare_Quit quit;
	if (!quit.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "quit parse fail");
		return;
	}

	uint64_t id = quit.task_id();
	
	LocationShareTask* task;
	auto it = m_localshare.find(id);
	if (it == m_localshare.end()) {
		return;
	}
	task = it->second;

	LocationShare_Quit_Ack ack;
	ack.set_errer_no(ERRNO_CODE_OK);
	ack.set_task_id(id);
	base.ResetPackBody(ack, LOCATIONSHARE_QUIT_ACK);
	sendConn(base);

	LocationShare_Quit_Notify notify;
	notify.set_allocated_quit_info(&quit);
	task->notifyUsers(this, &BuddyServer::notifyUsers, notify, LOCATIONSHARE_QUIT_NOTIFY);
	notify.release_quit_info();
	m_localshare.erase(it);
	delete task;
}

/*void BuddyServer::ProcessLocationShareContinue(int sockfd, SPDUBase & base)
{
	LocationShare share;
	if (!share.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "quit parse fail");
		return;
	}

	uint64_t id = share.task_id();
	
	LocationShareTask* task;
	auto it = m_localshare.find(id);
	if (it == m_localshare.end()) {
		return;
	}
	task = it->second;

	LocationShare_Notify notify;
	notify.set_allocated_location_share(&share);
	task->notifyUsers(this, &BuddyServer::notifyUsers, notify, LOCATIONSHARE_NOTIFY);
	notify.release_location_share();

}*/

void BuddyServer::ProcessLocationShareContinue(int sockfd, SPDUBase & base)
{
	LocationShare share;
	if (!share.ParseFromArray(base.body.get(), base.length)) {
		LOGERROR(base.command_id, base.seq_id, "Continue parse fail");
		return;
	}
	LocationShare_Notify notify;
	notify.set_allocated_location_share(&share);
	base.ResetPackBody(notify, LOCATIONSHARE_NOTIFY);
	base.seq_id = 0;
	int user_id = share.user_id();
	user_t* user = m_user_cache.findUser(user_id);
	if (user) {
		if (user->online) {
			base.terminal_token = user->user_id;
			base.node_id = user->nid;
			base.sockfd = user->fd;
			sendConn(base);
		}
	}
	notify.release_location_share();

}


/*..............broadcast..........................................*/
void BuddyServer::ProcessIMChat_broadcast(int sockfd, SPDUBase & base)
{
    TransferBroadcastNotify broadcast;
    if (!broadcast.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "Broadcast parse fail");
        return;
    }

    std::string msg_id = broadcast.msg_id();
    auto& body = broadcast.body();
    IMChat_Personal_Notify im_notify;

    if (!im_notify.ParseFromArray(body.c_str(), body.length())) {
        LOGERROR(base.command_id, base.seq_id, "Broadcast parse fail");
        return;
    }
    LOGI("recv broadcat chat msg [msg_id:%s]", msg_id.c_str());

    std::string channel_msg = "channel_msg:" + msg_id;
    ChatMsg* im = im_notify.mutable_imchat();
    if (!im->has_body()) {
        LOGE("not body");
    }

    const auto& user_list = broadcast.user_id_list();
    for (auto it = user_list.begin(); it != user_list.end(); it++) {
        user_t* user = m_user_cache.queryUser(*it);

        if (user) {
            if (user->online) {
                LOGI("broadcast chat msg to user[ph:%s]", user->phone);
                im->set_target_user_id(*it);
                im->set_target_phone(user->phone);
                base.terminal_token = *it;
                base.sockfd = user->fd;
                base.node_id = user->nid;
                ResetPackBody(base, im_notify, IMCHAT_PERSONAL);
                if (user->version == VERSION_0) {
                    sendConn(base);
                }
                else {
                    need_send_msg(user, base, atoi(msg_id.c_str()));
                }
                continue;
            }
            //user offline
            user->has_offline_msg = true;
        }
        LOGI("save broadcast offline msg user_id:%d", *it);
        std::string userid = "userid:" + std::to_string(*it);
        m_storage_msg_works->enqueue(&BuddyServer::storage_broadoffline_msg, this, userid, channel_msg);
    }
}

void BuddyServer::ProcessBulletin_broadcast(int sockfd, SPDUBase & base)
{
    TransferBroadcastNotify broadcast;
    if (!broadcast.ParseFromArray(base.body.get(), base.length)) {
        LOGERROR(base.command_id, base.seq_id, "Broadcast parse fail");
        return;
    }

    std::string msg_id = broadcast.msg_id();
    auto& body = broadcast.body();
    std::shared_ptr<char> pbody(new char[body.length()], carray_deleter);
    memcpy(pbody.get(), body.c_str(), body.length());
    base.body = pbody;
    base.length = body.length();
    LOGI("recv broadcat bulletin  msg [msg_id:%s]", msg_id.c_str());
    std::string channel_msg = "channel_msg:" + msg_id;
    Bulletin_Notify  notify;
    if (!notify.ParseFromArray(body.c_str(), body.length())) {
        LOGE("broadcast bulletin parse fail");
        return;
    }
    const auto& user_list = broadcast.user_id_list();
    for (auto it = user_list.begin(); it != user_list.end(); it++) {
        user_t* user = m_user_cache.queryUser(*it);
        if (user) {
            if (user->online) {

                LOGI("broadcast bulletin  msg to user_id(%d)", *it);
                base.terminal_token = *it;
                base.sockfd = user->fd;
                base.node_id = user->nid;
                if (user->version == VERSION_0 || user->version == VERSION_1 && base.command_id == BULLETIN_NOTIFY) {
                    sendConn(base);
                }
                else {
                    need_send_msg(user, base, atoi(msg_id.c_str()));
                }
                continue;
            }
            //user offline
            user->has_offline_msg = true;
        }
        LOGD("save bulletin offline msg user_id(%d)", *it);
        std::string userid = "userid:" + std::to_string(*it);
        m_storage_msg_works->enqueue(&BuddyServer::storage_broadoffline_msg, this, userid, channel_msg);
    }
}
#endif



void BuddyServer::ResetPackBody(SPDUBase &_pack, google::protobuf::Message &_msg, int _commandid) {
    std::shared_ptr<char> body(new char[_msg.ByteSize()], carray_deleter);
    _msg.SerializeToArray(body.get(), _msg.ByteSize());
    _pack.body = body;
    _pack.length = _msg.ByteSize();
    _pack.command_id = _commandid;
}


uint64_t BuddyServer::getMsgId()
{
    uint64_t msg_id;
    std::lock_guard<std::recursive_mutex> lock1(m_msgid_mutex_);

    time_t now = time(NULL);
    msg_id = (uint64_t)now << 32 | (uint64_t)m_node_id_ << 24;
    if (now != m_lastTime) {
        m_cur_index_ = 0;
        m_lastTime = now;
    }
    ++m_cur_index_;
    msg_id |= (m_cur_index_ & 0xffffff);//m_count shoud small 256*256*256;

    return msg_id;
}


bool BuddyServer::InsertMsgIdtoRedis(const std::string& user_id, const std::string& id) {

	const char *command[3];
	size_t vlen[3];

	command[0] = "SADD";
	command[1] = user_id.c_str();
	command[2] = id.c_str();
	vlen[0] = 4;
	vlen[1] = user_id.length();
	vlen[2] = id.length();

	bool res = storage_offline_msg_redis_.query(command, sizeof(command) / sizeof(command[0]), vlen);
	if (!res) {
		LOGE("buddy: storage msg fail");
	}
}

void BuddyServer::storage_common_msg(const std::string* data)
{
	std::string key = "Imlist";
	const char *command[3];
	size_t vlen[3];

	command[0] = "RPUSH";
	command[1] = key.c_str();
	command[2] = data->data();
	vlen[0] = 5;
	vlen[1] = key.length();
	vlen[2] = data->length();

	bool res = storage_msg_redis_.query(command, sizeof(command) / sizeof(command[0]), vlen);
	if (!res) {
		LOGE("buddy: storage msg fail");
	}
    delete data;
}

void BuddyServer::storage_apple_msg(const std::string * data)
{
   //redis_client.InsertIMPushtoRedis(*data);
    delete data;
}

void BuddyServer::storage_broadoffline_msg(std::string user, std::string msg_id)
{
    //redis_client.InsertBroadcastOfflineIMtoRedis(user, msg_id);
}


/*
 * 小的封装函数，只组合IM消息结果的包，发送给用户．
 */
void BuddyServer::ReplyChatResult(int sockfd, SPDUBase &_pack, ERRNO_CODE _code, bool is_target_online, uint64_t msg_id) {
	ChatMsg_Ack im_ack;
    im_ack.set_errer_no(_code);
    im_ack.set_msg_id(msg_id);
   
    std::shared_ptr<char> new_body(new char[im_ack.ByteSize()], carray_deleter);
    im_ack.SerializeToArray(new_body.get(), im_ack.ByteSize());
    _pack.body = new_body;
    _pack.length = im_ack.ByteSize();
    _pack.command_id = CID_CHAT_MSG_ACK;
    sendConn(_pack);
}

LocationShareTask::~LocationShareTask()
{
	auto it = user_list_.begin();
	while (it!=user_list_.end()) {
		delete *it;
		it=user_list_.erase(it);
	}
}

void LocationShareTask::addInviteUser(uint64_t user_id, const std::string phone)
{
	User* user = new User;
	user->user_id_ = user_id;
	user->phone_ = phone;
	user_list_.push_back(user);
}

void LocationShareTask::addAnswerUser(uint64_t user_id)
{
	auto it = user_list_.begin();
	for (it; it != user_list_.end(); it++) {
		if ((*it)->user_id_ == user_id) {
			(*it)->answer_ = true;
			break;
		}
	}
}

void LocationShareTask::notifyUsers(BuddyServer* msg_server, Fnotify cb, const google::protobuf::Message &_msg,int cmd)
{
	auto it = user_list_.begin();
	for (it; it != user_list_.end(); it++) {
		if ((*it)->answer_) {
			(msg_server->*cb)((*it)->user_id_, (*it)->phone_, _msg,cmd);
		}
	}
}
}
