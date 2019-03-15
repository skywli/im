#include "group_server.h"
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
#include <IM.Basic.pb.h>
#include <IM.Group.pb.h>
#include <IM.Msg.pb.h>
#include <IM.Redis.pb.h>
#include "common/event/sd_event_loop.h"
using namespace com::proto::basic;
using namespace com::proto::group;
using namespace com::proto::msg;

using namespace com::proto::state;
using namespace com::proto::server;
using namespace com::proto::redis;

static int sec_send_pkt;
int total_send_pkt;
namespace group {

	typedef ::google::protobuf::RepeatedPtrField< ::std::string> google_list_us;
	static const int db_index_offline_msg = 15;
	static  const int db_index_im_msg = 2;
	static const int db_index_group = 7;


	struct GroupServer::CmdTable {
		int cmd;
		handlerCb cb;
	} * pCmdHandler;
	GroupServer::GroupServer():m_loop(getEventLoop()),tcpService_(this){
		m_cur_index_ = 0;
        m_node_id_=0;
		m_lastTime = 0;
		m_pNode = CNode::getInstance();
		m_dbproxy_fd = -1;
		
		m_loop->init(1024);
		tcpService_.init(m_loop);
	}

	int GroupServer::sendConn(SPDUBase& base)
	{
		if (NodeMgr::getInstance()->sendNode(SID_CONN, base.node_id, base) == -1) {
			LOGE("send node[sid:%s,nid:%d]  cmd[%d] fail", sidName(SID_CONN), base.node_id, base.command_id);
		}
	}
	GroupServer * GroupServer::getInstance()
	{
		static GroupServer instance;
		return &instance;
	}

	int GroupServer::init()
	{
		m_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
		m_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
		if (m_ip == "" || m_port == 0) {
			LOGD("not config ip or port");
			return -1;
		}
		std::string redis_ip = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_IP);
		short redis_port = ConfigFileReader::getInstance()->ReadInt(CONF_REDIS_PORT);
		std::string auth = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_AUTH);
		LOGD("redis ip:%s,port:%d", redis_ip.c_str(), redis_port);
		m_user_cache.init();

		if (storage_offline_msg_redis_.connect(redis_ip, redis_port, auth, db_index_offline_msg) != 0) {
			LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
			return -1;
		}
		if (group_redis_.connect(redis_ip, redis_port, auth, db_index_group) != 0) {
			LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
			return -1;
		}
		if (storage_msg_redis_.connect(redis_ip, redis_port, auth, db_index_im_msg) != 0) {
			LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
			return -1;
		}
		//m_common_msg_process.init(ProcessClientMsg, 2);

		//    m_route_works = new ThreadPool(2); //handler route msg;
		m_storage_msg_works = new ThreadPool(1);//save msg redis;
												/*std::string file = ConfigFileReader::getInstance()->ReadString("statistics");
												if (file != "") {
												m_statistics.init(file);
												}*/
		m_chat_msg_process.init(ProcessClientMsg);

		
		std::string dbproxy_ip = ConfigFileReader::getInstance()->ReadString(CONF_DBPROXY_IP);
		short dbproxy_port = ConfigFileReader::getInstance()->ReadInt(CONF_DBPROXY_PORT);
		m_dbproxy_fd = tcpService_.connect(dbproxy_ip, dbproxy_port);
		if (m_dbproxy_fd == -1) {
			LOGE("connect dbproxy(addr: %s:%d) fail", dbproxy_ip.c_str(), dbproxy_port);
			//		return -1;
		}
		
		//CreateTimer(1000, Timer, this);
		m_pNode->init(&tcpService_);
		NodeMgr::getInstance()->init(m_loop, innerMsgCb, this);
		NodeMgr::getInstance()->setConnectionStateCb(connectionStateEvent, this);
		m_loop->createTimeEvent(1000, Timer, this);
		initCmdTable();

	}
	int GroupServer::initCmdTable() {
		static CmdTable cmdHander[] = {
			{ CID_CHAT_GROUP ,&GroupServer::ProcessIMChat_Group },
			{ CID_GROUP_LIST_REQ ,&GroupServer::groupListReq },
			{ CID_GROUP_LIST_RSP ,&GroupServer::groupListRsp },
			{ CID_GROUP_INFO_REQ ,&GroupServer::groupInfoReq },
			{ CID_GROUP_INFO_RSP ,&GroupServer::groupInfoRsp },
			{ CID_GROUP_MEMBER_REQ ,&GroupServer::groupMemberListReq },
			{ CID_GROUP_MEMBER_RSP ,&GroupServer::groupMemberListRsp },
			{ CID_GROUP_CREATE_REQ ,&GroupServer::createGroupReq },
			{ CID_GROUP_CREATE_RSP ,&GroupServer::createGroupRsp },
			{ CID_GROUP_DISSOLVE_REQ ,&GroupServer::dissolveGroupReq },
			{ CID_GROUP_DISSOLVE_RSP ,&GroupServer::dissolveGroupRsp },
			{ CID_GROUP_INFO_MODIFY_REQ ,&GroupServer::groupInfoModifyReq },
			{ CID_GROUP_INFO_MODIFY_RSP ,&GroupServer::groupInfoModifyRsp },
			{ CID_GROUP_CHANGE_MEMBER_REQ ,&GroupServer::changeMemberReq },
			{ CID_GROUP_CHANGE_MEMBER_RSP ,&GroupServer::changeMemberRsp },
			{ CID_S2S_GROUP_MEMBER_RSP ,&GroupServer::queryGroupmemberRsp },
			{ CID_GROUP_SET_REQ ,&GroupServer::groupSetReq },
			{ CID_GROUP_SET_RSP ,&GroupServer::groupSetRsp },
			{ CID_GROUP_CHANGE_MEMBER_TRANSFER_REQ ,&GroupServer::changeMemberTransferReq },
			{ CID_GROUP_CHANGE_MEMBER_TRANSFER_RSP ,&GroupServer::changeMemberTransferRsp },
			{ -1,NULL }
		};
		pCmdHandler = cmdHander;
	}

	void GroupServer::Timer(int fd, short mask, void * privdata)
	{
		GroupServer* pInstance = reinterpret_cast<GroupServer*>(privdata);
		if (pInstance) {
			pInstance->cron();
		}
	}
	void GroupServer::cron()
	{
		if (m_dbproxy_fd == -1) {
			std::string dbproxy_ip = ConfigFileReader::getInstance()->ReadString(CONF_DBPROXY_IP);
			short dbproxy_port = ConfigFileReader::getInstance()->ReadInt(CONF_DBPROXY_PORT);
			if (dbproxy_ip != "") {
				m_dbproxy_fd = tcpService_.connect(dbproxy_ip, dbproxy_port);
				if (m_dbproxy_fd == -1) {
					LOGE("connect dbproxy(addr: %s:%d) fail", dbproxy_ip.c_str(), dbproxy_port);
				}
			}
		}
		
		std::lock_guard<std::recursive_mutex> lock_1(m_msglock);
		//处理缓存消息
		map<uint32_t, list<MHandler*> > ::iterator it = m_msgHandler.begin();
		for (it; it != m_msgHandler.end();) {

			list<MHandler*>& li = it->second;
			list<MHandler*>::iterator lit = li.begin();
			for (lit; lit != li.end(); ) {
				MHandler* pHandler = *lit;
				TIME_T now;
				getCurTime(&now);
				if (interval(pHandler->time, now) > 15*1000) {
					LOGE("long time handler group(group_id:%u) member req,will drop it", it->first);
					std::lock_guard<std::recursive_mutex> lock_2(m_memberlock);
					m_queryMember.erase(it->first);
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

	void GroupServer::start() {
		m_chat_msg_process.start();
		sleep(1);
		LOGD("GroupServer listen on %s:%d", m_ip.c_str(), m_port);
		tcpService_.listen(m_ip, m_port);
		tcpService_.run();
	}

	void GroupServer::connectionStateEvent(NodeConnectedState* state, void* arg)
	{
		GroupServer* server = reinterpret_cast<GroupServer*>(arg);
		if (!server) {
			return;
		}
		if (state->sid == SID_DISPATCH && state->state == SOCK_CONNECTED) {
			server->registCmd(state->sid, state->nid);
		}
	}

	void GroupServer::registCmd(int sid, int nid)
	{
		LOGD("registcmd %s", sidName(sid));
		RegisterCmdReq req;
		m_node_id_ = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
		int cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
		req.set_node_id(m_node_id_);
		req.set_service_type(cur_sid);
		req.add_cmd_list(CID_CHAT_GROUP);
		req.add_cmd_list(CID_GROUP_LIST_REQ);
		req.add_cmd_list(CID_GROUP_INFO_REQ);
		req.add_cmd_list(CID_GROUP_MEMBER_REQ);
		req.add_cmd_list(CID_GROUP_CREATE_REQ);
		req.add_cmd_list(CID_GROUP_DISSOLVE_REQ);
		req.add_cmd_list(CID_GROUP_INFO_MODIFY_REQ);
		req.add_cmd_list(CID_GROUP_CHANGE_MEMBER_REQ);
		req.add_cmd_list(CID_GROUP_SET_REQ);
		req.add_cmd_list(CID_GROUP_CHANGE_MEMBER_TRANSFER_RSP);

		//video
		req.add_cmd_list(CID_VIDEO_ROOM_CREATE_REQ);
		req.add_cmd_list(CID_VIDEO_MEMBER_INVITE_REQ);
		req.add_cmd_list(CID_VIDEO_MEMBER_INVITE_REPONSE_REQ);
		req.add_cmd_list(CID_VIDEO_MEMBER_DELETE_REQ);
		req.add_cmd_list(CID_VIDEO_ROOM_INFO_REQ);
		req.add_cmd_list(CID_VIDEO_SETTING_REQ);
		req.add_cmd_list(CID_VIDEO_SETTING_APPLY_REQ);
		req.add_cmd_list(CID_VIDEO_PERMISSION_SETTING_REQ);
		req.add_cmd_list(CID_VIDEO_MEMBER_APPLY_REQ);
		req.add_cmd_list(CID_VIDEO_MEMBER_APPLY_REPONSE_REQ);
		req.add_cmd_list(CID_VIDEO_ROOM_INFO_REQ);
		req.add_cmd_list(CID_VIDEO_ROOM_DESTROY_REQ);
		req.add_cmd_list(CID_VIDEO_STATE_REQ);
		req.add_cmd_list(CID_VIDEO_ROOM_EXIT_REQ);
		req.add_cmd_list(CID_VIDEO_PING);
		SPDUBase spdu;
		spdu.ResetPackBody(req, CID_REGISTER_CMD_REQ);
		NodeMgr::getInstance()->sendNode(sid, nid, spdu);
	}

	void GroupServer::onData(int sockfd, PDUBase* base) {
		SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
		int cmd = spdu->command_id;

		switch (cmd) {

		case CID_S2S_AUTHENTICATION_REQ:
		case CID_S2S_PING:
			m_pNode->handler(sockfd, *spdu);
			delete base;
			break;
		default:
			m_chat_msg_process.addJob(sockfd, spdu);
			break;
        }
	}

	void GroupServer::onEvent(int _sockfd, ConnectionEvent event)
	{
		if (event == Disconnected) {
			if (sockfd == m_dbproxy_fd) {
				m_dbproxy_fd = -1;
			}
			m_pNode->setNodeDisconnect(sockfd);
			closeSocket(sockfd);
		}
	}

	/* ................handler msg recving from dispatch.............*/
	int GroupServer::processInnerMsg(int sockfd, SPDUBase & base)
	{
		m_chat_msg_process.addJob(sockfd, &base);
		return 0;
	}

	int GroupServer::innerMsgCb(int sockfd, SPDUBase & base, void * arg)
	{
		GroupServer* server = reinterpret_cast<GroupServer*>(arg);
		if (server) {
			server->processInnerMsg(sockfd, base);
		}
	}

	void GroupServer::ProcessClientMsg(int sockfd, SPDUBase* base)
	{
		GroupServer* pInstance = GroupServer::getInstance();
		//check user whether onlines;
		std::string user_id(base->terminal_token, sizeof(base->terminal_token));
		int cmd = base->command_id;
		int i = 0;
		while (pCmdHandler[i].cb) {
			if (pCmdHandler[i].cmd == cmd) {
				(pInstance->*(pCmdHandler[i].cb))(sockfd, *base);
				break;
			}
			++i;
		}
		if (!pCmdHandler[i].cb) {
			LOGE("unknow cmd:%d", cmd);
		}
		delete base;
	}

	void GroupServer::UserstateBroadcast(SPDUBase & _base)
	{
		UserStateBroadcast state_broadcast;
		if (!state_broadcast.ParseFromArray(_base.body.get(), _base.length)) {
			LOGERROR(_base.command_id, _base.seq_id, "UserStateBroadcast parse fail");
			return;
		}
		const std::string  user_id = state_broadcast.user_id();
		user_t* user = m_user_cache.findUser(user_id);
		if (!user) {
			user = m_user_cache.getUser(user_id);
		}
		if (user) {
			user->nid = state_broadcast.node_id();
			user->fd = state_broadcast.sockfd();
			int state= state_broadcast.state();
			user->online = state == USER_STATE_ONLINE ? true : false;
			user->version = state_broadcast.version();
			user->device_type = state_broadcast.device_type();
		}
		else {
			LOGE("state broadcast error");
			return;
		}
		LOGD("state broadcast,user[user_id:%s nid:%d,fd:%d] %s", user->user_id.c_str(), user->nid, user->fd, user->online ? "online" : "offline");

		if (user->online) {
			user->online_time = state_broadcast.time();
		}
	}

	int GroupServer::getUserGroupList(user_t* user) {
		std::string& user_id = user->user_id;
		std::string key = "ugl_";
		key += user_id;
		const char *command[2];
		size_t vlen[2];

		command[0] = "get";
		command[1] = key.c_str();

		vlen[0] = 3;
		vlen[1] = key.length();

		std::string value;
		bool res = group_redis_.query(command, sizeof(command) / sizeof(command[0]), vlen, value);
		if (!res) {
			return -1;
		}
		if (memcmp(value.data(), "NO", 2) == 0) {
			return 0;
		}
		RDUserGroupList user_group_list;
		if (!user_group_list.ParseFromArray(value.data(), value.length())) {
			LOGE("RDUserGroupList parse fail");
			return -1;
		}

		const auto& group_list = user_group_list.group_item_list();
		auto it = group_list.begin();
		for (it; it != group_list.end(); it++) {
			uint32_t group_id = it->group_id();
			user->group_list.push_back(group_id);
		}
		return 1;
	}
	bool GroupServer::getGroupInfo(uint32_t group_id, GroupInfo& group_info) {

		std::string key = "gi_";
		key += std::to_string(group_id);
		const char *command[2];
		size_t vlen[2];

		command[0] = "get";
		command[1] = key.c_str();

		vlen[0] = 3;
		vlen[1] = key.length();

		std::string value;

		bool res = group_redis_.query(command, sizeof(command) / sizeof(command[0]), vlen, value);
		if (!res) {
			return false;
		}
		RDGroupInfo rd_group_info;
		if (!rd_group_info.ParseFromArray(value.data(), value.length())) {
			LOGE("RDGroupInfo parse fail");
			return false;
		}

		group_info = rd_group_info.group_info();

		return true;
	}

	typedef ::google::protobuf::RepeatedPtrField< ::com::proto::group::GroupItem > group_item_list_t;
	static int compareGroupInfoVersion(const group_item_list_t&group_item_list, uint32_t group_id, uint64_t update_time) {
		auto it = group_item_list.begin();
		for (it; it != group_item_list.end(); it++) {
			if (it->group_id() == group_id) {
				return it->info_update_time() - update_time;
			}
		}
		return -1;
	}

	bool GroupServer::groupListReq(int sockfd, SPDUBase&  base)
	{
		GroupListReq req;
		if (!req.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupListReq parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		const std::string& user_id = req.user_id();
		LOGD("user[%s] group list req",user_id.c_str());
		GroupListRsp rsp;

		user_t* user = m_user_cache.queryUser(user_id);
		if (user->group_list.size() == 0) {
			if (-1==getUserGroupList(user)) {
				LOGD("get grouplist fail,send db send db get grouplist");
				tcpService_.Send(m_dbproxy_fd, base);
				return true;
			}
		}
		//get every group info
		const auto& group_item_list = req.group_item_list();
		auto it = user->group_list.begin();

		for (it; it != user->group_list.end(); it++) {
			Group* pGroup = m_group_cache.findGroup(*it);
			//we add a group but not include group member
			if (!pGroup) {
				pGroup = m_group_cache.createGroup(*it);
			}
			if (pGroup) {
				uint64_t update_time = pGroup->getGroupInfoUpdateTime();
				int res = compareGroupInfoVersion(group_item_list, *it, update_time);
				//if cache not record groupinfo,send db query groupinfo
				if (res != 0 || update_time == 0) {
					LOGD("begin update group[%d] info", *it);
					GroupInfo group_info;
					if (!getGroupInfo(*it, group_info)) {
						LOGD("get group[%d] info fail, send db get grouplist", *it);
						Send(m_dbproxy_fd, base);
						return true;
					}
					//cache must update
					if (res > 0 || update_time == 0) {
						pGroup->setGroupInfoUpdateTime(group_info.info_update_time());
					}
					GroupInfo* pGInfo = rsp.add_group_info_list();
					pGInfo->CopyFrom(group_info);
				}

			}

		}
		//delete group
		auto item = group_item_list.begin();
		while (item != group_item_list.end()) {
			auto it = std::find(user->group_list.begin(), user->group_list.end(), item->group_id());
			if (it == user->group_list.end()) {
				LOGD("delete user[%s]  group[%d] ", user_id.c_str(),item->group_id());
				rsp.add_delete_group_id_list(item->group_id());
			}
			++item;
		}

		LOGD("reply group list rsp");
		rsp.set_result(RESULT_CODE_SUCCESS);
		ResetPackBody(base, rsp, CID_GROUP_LIST_RSP);
		sendConn(base);
		return true;
	}

	bool GroupServer::groupListRsp(int sockfd, SPDUBase&  base)
	{
		LOGD("group list rsp");
		GroupListRsp rsp;
		if (!rsp.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupListRsp parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		sendConn(base);
		return true;
	}

	bool GroupServer::groupInfoReq(int sockfd, SPDUBase&  base)
	{
		GroupInfoReq req;
		if (!req.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupListReq parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		GroupInfoRsp rsp;
		uint32_t group_id = req.group_id();
		LOGD("GroupInfoReq[%d]",group_id);
		Group* pGroup = m_group_cache.findGroup(group_id);
		//we add a group but not include group member
		if (!pGroup) {
			pGroup = m_group_cache.createGroup(group_id);
		}
		ResultCode result = RESULT_CODE_SUCCESS;
		if (pGroup->empty()) {
			if (!getGroupMember(pGroup)) {
				groupmemberReq(group_id, sockfd, base, &GroupServer::groupInfoReq);
				return false;
			}
		}
		//user not in group
		if (!pGroup->hasMember(req.user_id())) {
			LOGE("user[%s] not in group[%d]", req.user_id().c_str(), group_id);
			result = RESULT_CODE_NOT_FIND;
		}
		uint64_t update_time = pGroup->getGroupInfoUpdateTime();
		int res = req.update_time() - update_time;
		//if cache not record group update_time
		if (res != 0 || update_time == 0) {
			LOGD("begin update group[%d] info", group_id);
			GroupInfo group_info;
			if (!getGroupInfo(group_id, group_info)) {
				LOGD("get group[%d] info fail,send db get groupinfo", group_id);
				Send(m_dbproxy_fd, base);
				return true;
			}
			//cache must update
			if (res > 0 || update_time == 0) {
				pGroup->setGroupInfoUpdateTime(group_info.info_update_time());
			}
			auto pGInfo = rsp.mutable_group_info();
			pGInfo->CopyFrom(group_info);
			rsp.set_update(true);
		}
		else {
			rsp.set_update(false);
		}
			
		LOGD("reply group info rsp");
		rsp.set_result(result);
		ResetPackBody(base, rsp, CID_GROUP_INFO_RSP);
		sendConn(base);

		return true;
	}
	bool GroupServer::groupInfoRsp(int sockfd, SPDUBase&  base)
	{
		LOGD("GroupInfoRsp");
		sendConn(base);
		return true;
	}

	bool GroupServer::getGroupMember(uint32_t group_id, RDGroupMemberList& rd_member_list) {

		std::string key = "gml_";
		key += std::to_string(group_id);
		const char *command[2];
		size_t vlen[2];

		command[0] = "get";
		command[1] = key.c_str();

		vlen[0] = 3;
		vlen[1] = key.length();

		std::string value;

		bool res = group_redis_.query(command, sizeof(command) / sizeof(command[0]), vlen, value);
		if (!res) {
			LOGE("get group member  fail");
			return false;
		}

		if (!rd_member_list.ParseFromArray(value.data(), value.length())) {
			LOGE("RDGroupMemberList parse fail");
			return false;
		}
		return true;
	}

	bool GroupServer::getGroupMember(Group* pGroup) {
		RDGroupMemberList  rd_member_list;
		if (getGroupMember(pGroup->getGroupId(), rd_member_list)) {
			pGroup->clear();
			const auto& member_list = rd_member_list.member_list();
			auto it = member_list.begin();
			pGroup->setMemberUpdateTime(rd_member_list.update_time());
			for (it; it != member_list.end(); it++) {
				pGroup->addMember(it->member_id(), it->member_role());
			}
			return true;
		}
		return false;
	}

	bool GroupServer::groupMemberListReq(int sockfd, SPDUBase&  base)
	{
		GroupMemberReq  req;
		if (!req.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupMemberReq parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		GroupMemberRsp rsp;
		uint32_t group_id = req.group_id();
		const UserId_t& user_id = req.user_id();
		uint64_t update_time = req.update_time();
		LOGD("user[%s ]GroupMemberListReq [%d]",user_id.c_str(),group_id);
		Group* pGroup = m_group_cache.findGroup(group_id);
		if (!pGroup) {
			pGroup = m_group_cache.createGroup(group_id);
		}
		rsp.set_group_id(group_id);
		rsp.set_result(RESULT_CODE_SUCCESS);

		if (update_time != 0 && pGroup->getMemberUpdateTime() == update_time) {
			//群用户信息无更改，暂不处理
			LOGD("time:%lu,group[group_id:%u] is latest", update_time, group_id);
			rsp.set_update_time(update_time);
		}
		else {
			LOGD("update group member");
			RDGroupMemberList  rd_member_list;
			if (getGroupMember(group_id, rd_member_list)) {
				pGroup->clear();
				const auto& member_list = rd_member_list.member_list();
				auto it = member_list.begin();
				pGroup->setMemberUpdateTime(rd_member_list.update_time());
				for (it; it != member_list.end(); it++) {
                    LOGD("add user(%s)",it->member_id().c_str());
					pGroup->addMember(it->member_id(), it->member_role());
				}
                LOGD("size:%d",member_list.size());
				auto pMember_list = rsp.mutable_member_list();
				pMember_list->CopyFrom(member_list);
				rsp.set_update_time(rd_member_list.update_time());
			}
			else {
				LOGE("get group[%d] member fail,send db get groupmember", group_id);
				tcpService_.Send(m_dbproxy_fd, base);
				return true;
			}
		}

        LOGD("reply groupmember list rsp");
		ResetPackBody(base, rsp, CID_GROUP_MEMBER_RSP);
		sendConn(base);
		return true;
	}

	bool GroupServer::groupMemberListRsp(int sockfd, SPDUBase&  base)
	{
		LOGD("GroupMemberListRsp");
		GroupMemberRsp rsp;
		if (!rsp.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupListRsp parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		uint32_t group_id = rsp.group_id();
		uint64_t update_time = rsp.update_time();

		Group* pGroup = m_group_cache.findGroup(group_id);
		if (pGroup) {
			pGroup->setMemberUpdateTime(update_time);
		}

		sendConn(base);
		return true;
	}

	bool GroupServer::createGroupReq(int sockfd, SPDUBase&  base)
	{
		LOGD("create group req");
		GroupCreateReq req;

		if (!req.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupCreateReq parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}

		const auto& member_list = req.member_list();
		for (int i = 0; i<member_list.size(); i++) {
			LOGD("user_id:%s", member_list[i].member_id().c_str());
		}
		
		tcpService_.Send(m_dbproxy_fd, base);
	}

	bool GroupServer::createGroupRsp(int sockfd, SPDUBase&  base)
	{
		LOGD("create group rsp");
		GroupCreateRsp rsp;

		if (!rsp.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupCreateRsp parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		//reply create group user
		sendConn(base);
		uint32_t group_id = rsp.group_id();

		const UserId_t&  owner_id = rsp.user_id();
		uint64_t update_time = rsp.update_time();

		ResultCode result = rsp.result();

		if (result != RESULT_CODE_SUCCESS) {
			LOGE("user[%s] create group fail", owner_id.c_str());
			return false;
		}
		LOGD("user[%s] create group[id(%u)] success", owner_id.c_str(), group_id);

		Group* pGroup = m_group_cache.createGroup(group_id);
		pGroup->setGroupInfoUpdateTime(update_time);
		pGroup->setMemberUpdateTime(update_time);
		
		pGroup->addMember(owner_id, GROUP_OWNER);
		
		//if user group list change,delete it until user querying  grouplist get group list from redis 
		user_t* user = m_user_cache.queryUser(owner_id); 
		user->group_list.clear();
		//发送通知
		GroupOptNotify  notify;
		notify.set_opt_type(0);
		notify.set_group_id(group_id);


		ResetPackBody(base, notify, CID_GROUP_OPT_NOTIFY);
		const auto& member_list = rsp.member_list();
		auto it = member_list.begin();
		//notify other user

		for (it; it != member_list.end(); ++it) {
			const UserId_t& user_id = it->member_id();
			
			if (owner_id == user_id) {
				continue;
			}
			LOGD("create group notify:user[%s]", user_id.c_str());
			Member* mem = pGroup->addMember(user_id, GROUP_COMMON);
			user_t* user = m_user_cache.queryUser(user_id);
			user->group_list.clear();
			if (user->online) {
				memcpy(base.terminal_token, user_id.c_str(), sizeof(base.terminal_token));
				base.node_id = user->nid;
				base.sockfd = user->fd;
				sendConn(base);
			}
		}
		return true;
	}

	bool GroupServer::dissolveGroupReq(int sockfd, SPDUBase&  base)
	{
		LOGD("Dissolve group rep");
		GroupDissolveReq  req;
		if (!req.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "dissolveGroupReq parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}

		GroupId_t group_id = req.group_id();
		Group* pGroup = m_group_cache.findGroup(group_id);
		if (!pGroup) {
			pGroup = m_group_cache.createGroup(group_id);
			getGroupMember(pGroup);
		}
		if (pGroup->empty()) {
			getGroupMember(pGroup);
		}
		//发给db
		tcpService_.Send(m_dbproxy_fd, base);
		return true;
	}

	bool GroupServer::dissolveGroupRsp(int sockfd, SPDUBase&  base)
	{
		LOGD("Dissolve group rsp");

		GroupDissolveRsp rsp;

		if (!rsp.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupDissolveRsp parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}

		uint32_t group_id = rsp.group_id();
		ResultCode result = rsp.result();
		sendConn(base);

		Group* pGroup = m_group_cache.findGroup(group_id);
		if (pGroup == NULL) {
			//group had been dissolved,don't query db
			LOGE("group_id(%u) not find", group_id);
			result = RESULT_CODE_FAIL;
		}

		if (RESULT_CODE_SUCCESS != result) {
			return false;
		}

		//发送通知
		GroupOptNotify  notify;
		notify.set_opt_type(1);
		notify.set_group_id(group_id);

		ResetPackBody(base, notify, CID_GROUP_OPT_NOTIFY);
		const list<Member*>& li = pGroup->getMemberList();
		list<Member*>::const_iterator it = li.begin();

		for (it; it != li.end(); ++it) {
			const UserId_t& user_id = (*it)->user_id;
			user_t* user = m_user_cache.queryUser(user_id);
			user->group_list.clear();
			if ((*it)->role == GROUP_OWNER) {
				continue;
			}
			if (user->online) {
				memcpy(base.terminal_token, user_id.c_str(), sizeof(base.terminal_token));
				base.node_id = user->nid;
				base.sockfd = user->fd;
				sendConn(base);
			}
		}
		m_group_cache.DissolveGroup(group_id);
		return true;
	}

	bool GroupServer::groupInfoModifyReq(int sockfd, SPDUBase&  base)
	{
		LOGD("groupInfoModifyReq");
		tcpService_.Send(m_dbproxy_fd, base);
		return true;
	}

	bool GroupServer::groupInfoModifyRsp(int sockfd, SPDUBase&  base)
	{
		LOGD("groupInfoModifyRsp");
		GroupInfoModifyRsp rsp;
		if (!rsp.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "groupInfoModifyRsp parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		ResultCode result = rsp.result();

		GroupId_t group_id = rsp.group_id();

		GroupInfo groupInfo = rsp.group_info();
		sendConn(base);

		if (result != RESULT_CODE_SUCCESS) {
			LOGE("GroupInfoModify fail");
			return false;
		}
		Group* pGroup = m_group_cache.findGroup(group_id);
		if (!pGroup) {
			pGroup = m_group_cache.createGroup(group_id);
		}
		pGroup->setGroupInfoUpdateTime(groupInfo.info_update_time());

		IMGroupInfoModifyNotify notify;
		notify.set_group_id(group_id);

		GroupInfo* pGroupInfo = notify.mutable_group_info();
		pGroupInfo->CopyFrom(groupInfo);
		notify.set_type(rsp.type());
		notify.set_user_id(base.terminal_token, sizeof(base.terminal_token));
		notify.set_src_owner_name(rsp.src_owner_name());
		notify.set_dst_owner_name(rsp.dst_owner_name());
        LOGE("dst owner:%s",rsp.dst_owner_name().c_str());
		ResetPackBody(base, notify, CID_GROUP_INFO_MODIFY__NOTIFY);
        if (pGroup->empty()) {
            getGroupMember(pGroup);
        }
		const list<Member*>& member_list = pGroup->getMemberList();
		list<Member*>::const_iterator it = member_list.begin();
		std::string owner(base.terminal_token, sizeof(base.terminal_token));
		for (it; it != member_list.end(); ++it) {
			const UserId_t& user_id = (*it)->user_id;
			//LogMsg(LOG_DEBUG, "send ack user[id: %u]", (*it)->user_id);
			//not notify 
			LOGD("modify group info  notify:user[%s]", user_id.c_str());
			user_t* user = m_user_cache.queryUser(user_id);
			if (user->online) {
				memcpy(base.terminal_token, user_id.c_str(), sizeof(base.terminal_token));
				base.node_id = user->nid;
				base.sockfd = user->fd;
				sendConn(base);
			}
		}
		return true;
	}


	bool GroupServer::changeMemberReq(int sockfd, SPDUBase&  base)
	{
		GroupMemberChangeReq req;
		if (!req.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupMemberChangeReq parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		uint32_t group_id = req.group_id();
		const UserId_t& user_id = req.user_id();
		LOGD("changeMember[group_id:%u,user_id:%s]  rep type[%u]", group_id, user_id.c_str(), req.type());
		Group* pGroup = m_group_cache.findGroup(group_id);

		if (pGroup == NULL) {
			pGroup = m_group_cache.createGroup(group_id);
			getGroupMember(pGroup);
				
		}
		if (pGroup->empty()) {
			getGroupMember(pGroup);
		}
		tcpService_.Send(m_dbproxy_fd, base);
	}

	bool GroupServer::changeMemberRsp(int sockfd, SPDUBase&  base)
	{
		LOGD("changeMember  rsp");
		Group* pGroup = NULL;
		GroupMemberChangeRsp rsp;

		if (!rsp.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupMemberChangeRsp parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		uint32_t group_id = rsp.group_id();
		ResultCode result = rsp.result();
		int type = rsp.type();
		sendConn(base);
		if (result != RESULT_CODE_SUCCESS) {
			return true;
		}

		pGroup = m_group_cache.findGroup(group_id);
		if (NULL == pGroup || pGroup->empty()) {
			//请求之前查找group,请求之后group为空（解散群）不处理
			return false;
		}
		pGroup->setMemberUpdateTime(rsp.update_time());
		pGroup->setGroupInfoUpdateTime(0);//clear
		int num = rsp.member_list_size();
		if (num > 0) {

			if (GROUP_MEMBER_OPT_ADD == type || GROUP_MEMBER_OPT_DEL == type) {

				const auto &  idItem = rsp.member_list();
				auto user_begin = idItem.begin();

				// first add member
				if (GROUP_MEMBER_OPT_ADD == type) {
					for (user_begin; user_begin != idItem.end(); ++user_begin) {
						pGroup->addMember((*user_begin).member_id(), GROUP_COMMON);
					}
				}

				//notify all member include deleted number
				GroupMemberChangeNotify notify;
				auto pMemberList = notify.mutable_member_list();
				notify.set_group_id(group_id);
				notify.set_type(rsp.type());
				notify.set_update_time(rsp.update_time());
				pMemberList->CopyFrom(rsp.member_list());
				ResetPackBody(base, notify, CID_GROUP_CHANGE_MEMBER_NOTIFY);


				const list<Member*>& member_list = pGroup->getMemberList();
				list<Member*>::const_iterator it = member_list.begin();
				for (it; it != member_list.end(); ++it) {
					const UserId_t& user_id = (*it)->user_id;
					
					user_t* user = m_user_cache.queryUser(user_id);
					user->group_list.clear();
					LOGD("change group member notify:user[%s]", user_id.c_str());
					if (user->online) {
						memcpy(base.terminal_token, user_id.c_str(), sizeof(base.terminal_token));
						base.node_id = user->nid;
						base.sockfd = user->fd;
						sendConn(base);
					}
				}

				//last del member
				if (GROUP_MEMBER_OPT_DEL == type) {
					user_begin = idItem.begin();
					for (user_begin; user_begin != idItem.end(); ++user_begin) {
						pGroup->delMember((*user_begin).member_id());
					}
				}
			}
		}
		return true;
	}

	bool GroupServer::AppendMsgIdtoRedis(const std::string& user_id, const std::string& id) {

		const char *command[3];
		size_t vlen[3];

		command[0] = "SADD";
		command[1] = user_id.c_str();
		command[2] = id.c_str();
		vlen[0] = 4;
		vlen[1] = user_id.length();
		vlen[2] = id.length();

		bool res = storage_offline_msg_redis_.appendCommandArgv(command, sizeof(command) / sizeof(command[0]), vlen);
		if (!res) {
			LOGE("group: add msg fail");
		}
	}

	bool GroupServer::ExecuteCmdRedis(int num) {
		
		int res = storage_offline_msg_redis_.getReply(num);
		if (res!=num) {
			LOGE("group: storage msg  fail,storage num:%d",res);
		}
	}

	bool GroupServer::InsertMsgIdtoRedis(const std::string& user_id, const std::string& id) {

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
			LOGE("group: storage msg fail");
		}
	}

	void  GroupServer::groupmemberReq( uint32_t group_id,int sockfd,SPDUBase& spdu, handlerCb cb)
	{
		std::lock_guard<std::recursive_mutex> lock_1(m_memberlock);

		MHandler* pHandler = new MHandler;
		
		//memset(&pHandler->msg, 0, sizeof(pHandler->msg));
		//	deep_copy(pHandler->msg, msg);
		pHandler->spdu = spdu;
		pHandler->fd = sockfd;
		pHandler->handler = cb;
		getCurTime(&pHandler->time);
		{
			std::lock_guard<std::recursive_mutex> lock_1(m_msglock);
			map<uint32_t, list<MHandler*> > ::iterator it = m_msgHandler.find(group_id);
			if (it != m_msgHandler.end()) {
				it->second.push_back(pHandler);
			}
			else {
				list<MHandler*> li;
				li.push_back(pHandler);
				m_msgHandler[group_id] = li;
			}
		}
		//避免重复查询
		if (m_queryMember.find(group_id) != m_queryMember.end()) {
			LOGD( "already query  group(%d)",group_id);
			return;
		}
		m_queryMember.insert(group_id);
	
		SPDUBase request;
		GroupMemberReq req;
		req.set_user_id("group");
		req.set_group_id(group_id);
		ResetPackBody(request, req, CID_S2S_GROUP_MEMBER_REQ);
		tcpService_.Send(m_dbproxy_fd, request);
	}

	bool GroupServer::queryGroupmemberRsp(int sockfd, SPDUBase&  base)
	{
		LOGD("GroupMemberListRsp");
		GroupMemberRsp rsp;
		if (!rsp.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupListRsp parse fail");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}
		uint32_t group_id = rsp.group_id();
		uint64_t update_time = rsp.update_time();
		int result = rsp.result();
		
		Group* pGroup = m_group_cache.findGroup(group_id);
		if (!pGroup) {
			pGroup=m_group_cache.createGroup(group_id);
		}
		do {
			pGroup->clear();
			//Setting group dissolve flag If group is dissolved;
			if (result != RESULT_CODE_SUCCESS) {
				pGroup->setDissolve();
				break;
			}
			const auto& member_list = rsp.member_list();
			
			auto it = member_list.begin();
			pGroup->setMemberUpdateTime(update_time);

			for (it; it != member_list.end(); it++) {
				pGroup->addMember(it->member_id(), it->member_role());
			}
		} while (0);
		std::lock_guard<std::recursive_mutex> lock_1(m_memberlock);
		m_queryMember.erase(group_id);
		handlerMsg( group_id);
		return true;
	}

	int GroupServer::handlerMsg( uint32_t group_id)
	{
		LOGD("handler group(%d) msg", group_id);
		std::lock_guard<std::recursive_mutex> lock_1(m_msglock);
		map<uint32_t, list<MHandler*> > ::iterator it = m_msgHandler.find(group_id);
		if (it != m_msgHandler.end()) {
			list<MHandler*>::iterator lit = it->second.begin();
			for (lit; lit != it->second.end();) {
				MHandler* pHandler = *lit;
				lit = it->second.erase(lit);
				handlerCb cb = pHandler->handler;
				(GroupServer::getInstance()->*cb)(pHandler->fd, pHandler->spdu);
				delete pHandler;
			}
			m_msgHandler.erase(it);
		}
		return 0;
	}

	
	bool GroupServer::groupSetReq(int sockfd, SPDUBase&  base) {
		LOGD("groupSetReq");
		tcpService_.Send(m_dbproxy_fd, base);
	}

	bool GroupServer::groupSetRsp(int sockfd, SPDUBase&  base) {
		LOGD("groupSetRsp");
		sendConn(base);
		return true;
	}
	
	bool GroupServer::changeMemberTransferRsp(int sockfd, SPDUBase&  base) {
		LOGD("changeMemberTransferRsp");
		tcpService_.Send(m_dbproxy_fd, base);
	}

	bool GroupServer::changeMemberTransferReq(int sockfd, SPDUBase&  base) {
		GroupMemberChangeTransferReq req;
		if (!req.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "GroupMemberChangeTransferReq parse fail");
			return false;
		}
		LOGD("changeMemberTransferReq");
		const auto& user_id = req.dst_user_id();
		int nid = NodeMgr::getInstance()->getUserNodeId(SID_MSG, user_id);
		if (nid == -1) {
			LOGE("get node fail");
			return false;
		}
		uint64_t msg_id = getMsgId();
		InsertMsgIdtoRedis(user_id, std::to_string(msg_id));
		S2SPushMsg push;
		PushMsg msg;

		msg.set_data(base.body.get(), base.length);
		push.add_user_id(user_id);
		msg.set_msg_id(msg_id);
		msg.set_cmd_id(CID_GROUP_CHANGE_MEMBER_REQ);
		push.set_allocated_msg(&msg);
		ResetPackBody(base, push, CID_PUSH_MSG);
		NodeMgr::getInstance()->sendNode(SID_MSG, nid, base);
		push.release_msg();
		return true;
	}
	/*
	* 处理用户发送的IM消息
	* 注意：用户发送消息时只有自己的userid和对方的手机号，所以其他信息需要查询获得
	*/
	bool GroupServer::ProcessIMChat_Group(int sockfd, SPDUBase&  base ) {
		ChatMsg im;
		Device_Type device_type = DeviceType_UNKNOWN;

		if (!im.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "IMChat_Personal包解析错误");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return false;
		}

		MsgData*  data = im.mutable_data();
		const std::string& src_user_id = data->src_user_id();
		uint32_t group_id = data->group_id();
		SessionType type = data->session_type();
		Group* pGroup = m_group_cache.findGroup(group_id);
		
		if (pGroup == NULL) {
			pGroup = m_group_cache.createGroup(group_id);
			if (!getGroupMember(pGroup)) {
				groupmemberReq(group_id, sockfd, base, &GroupServer::ProcessIMChat_Group);
				return false;
			}
		}
		if (pGroup->dissolve()) {
			ReplyChatResult(sockfd, base, ERRNO_CODE_USER_NOT_IN_GROUP, 0, 0);
			return false;
		}
		if (pGroup->empty()) {
			if (!getGroupMember(pGroup)) {
				groupmemberReq(group_id, sockfd, base, &GroupServer::ProcessIMChat_Group);
				return false;
			}
		}
		//user not in group
		if (!pGroup->hasMember(src_user_id)) {
            LOGE("user[%s] not in group[%d]",src_user_id.c_str(),group_id);
			ReplyChatResult(sockfd, base, ERRNO_CODE_USER_NOT_IN_GROUP, 0, 0);
			return false;
		}
		// 以服务器收到消息的时间为准
		data->set_create_time(timestamp_int());
		uint64_t msg_id = getMsgId();
		int content_type = data->content_type();
		//	m_statistics.addRecvMsg(msg_id, src_user_id, dest_user_id, get_mstime(), data->content_type());
		LOGI("IM type:%d，from:%s to:%d (msg_id:%ld)", content_type, src_user_id.c_str(), group_id, msg_id);
		if (content_type == 0) {
			  LOGD("content msg:%s", data->msg_content().c_str());
		}
		bool is_target_online = 0;

		ReplyChatResult(sockfd, base, ERRNO_CODE_OK, is_target_online, msg_id);


		data->set_msg_id(msg_id);
		ResetPackBody(base, im, CID_CHAT_GROUP);


		std::string* msg_data = new std::string(base.body.get(), base.length);

		m_storage_msg_works->enqueue(&GroupServer::storage_common_msg, this, msg_data);


		//LOGDEBUG(base.command_id, base.seq_id, "推送%d", device_type);
		if (device_type == DeviceType_IPhone || device_type == DeviceType_IPad) {

			//m_storage_msg_works->enqueue(&GroupServer::storage_apple_msg, this, msg_data);
		}
		
		if (content_type == IM_CONTENT_TYPE_GET_RED_ENVELOPE) {
			const std::string& dest_user_id = data->dest_user_id();
			InsertMsgIdtoRedis(dest_user_id, std::to_string(msg_id));
			int nid = NodeMgr::getInstance()->getUserNodeId(SID_MSG, dest_user_id);
			int cur_nid = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
			if (cur_nid != nid) {
				S2SPushMsg push;
				PushMsg msg;
				msg.set_data(base.body.get(), base.length);
				push.add_user_id(dest_user_id);
				msg.set_msg_id(msg_id);
				msg.set_cmd_id(CID_CHAT_GROUP);
				push.set_allocated_msg(&msg);
				ResetPackBody(base, push, CID_PUSH_MSG);

				NodeMgr::getInstance()->sendNode(SID_MSG, nid, base);
				push.release_msg();
			}
			return true;
		}
		
		const std::string& self= src_user_id;
		const list<Member*>& member_list = pGroup->getMemberList();
		list<Member*>::const_iterator it = member_list.begin();

		int num=0;
		//first insert to redis
		for (it; it != member_list.end(); ++it) {
			const UserId_t& user_id = (*it)->user_id;
			if (user_id == self) {
				continue;
			}
			++num;
           LOGD("send user_id[%s]",user_id.c_str());
			AppendMsgIdtoRedis(user_id, std::to_string(msg_id));
		}
		if (num) {
			ExecuteCmdRedis(num);
		}
		

		//send to msg server
		std::map<int, google_list_us> mli;
		it = member_list.begin();
		int i = 0;
		for (it; it != member_list.end(); ++it) {

			const UserId_t& user_id = (*it)->user_id;
			//req self
			if (user_id == self) {
				continue;
			}

			int nid = NodeMgr::getInstance()->getUserNodeId(SID_MSG, user_id);
			if (nid == -1) {
				LOGE("get node fail");
				continue;
			}
			auto mit = mli.find(nid);
			if (mit == mli.end()) {
				google_list_us uli;
				auto pItem = uli.Add();
				*pItem = user_id;
				mli.insert(std::pair<int, google_list_us>(nid, uli));
				++i;
			}
			else {
				auto pItem = mit->second.Add();
				*pItem = user_id;
				++i;
			}

		}

		{
			auto it = mli.begin();
			for (it; it != mli.end(); it++) {
				LOGD("send group(%d) msg to nid(%d) total(%d) users", group_id, it->first, it->second.size());
				S2SPushMsg push;
				PushMsg msg;

				msg.set_data(base.body.get(), base.length);
				auto user_id_list = push.mutable_user_id();
				user_id_list->CopyFrom(it->second);
				msg.set_msg_id(msg_id);
				msg.set_cmd_id(CID_CHAT_GROUP);
				push.set_allocated_msg(&msg);
				ResetPackBody(base, push, CID_PUSH_MSG);
				NodeMgr::getInstance()->sendNode(SID_MSG, it->first, base);
				push.release_msg();
			}
		}
		return true;
	}



	void GroupServer::ResetPackBody(SPDUBase &_pack, google::protobuf::Message &_msg, int _commandid) {
		std::shared_ptr<char> body(new char[_msg.ByteSize()], carray_deleter);
		_msg.SerializeToArray(body.get(), _msg.ByteSize());
		_pack.body = body;
		_pack.length = _msg.ByteSize();
		_pack.command_id = _commandid;
	}


	uint64_t GroupServer::getMsgId()
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


	/*
	* 小的封装函数，只组合IM消息结果的包，发送给用户．
	*/
	void GroupServer::ReplyChatResult(int sockfd, SPDUBase &_pack, ERRNO_CODE _code, bool is_target_online, uint64_t msg_id) {
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

	void GroupServer::storage_common_msg(const std::string* data)
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
}
