#include "msg_server.h"
#include "log_util.h"
#include "config_file_reader.h"
#include "IM.Msg.pb.h"
#include "time_util.h"
#include "logic_util.h"
#include <unistd.h>
#include "Base64.h"
#include <node_mgr.h>
#include <string.h>
#include "IM.State.pb.h"
#include <node_mgr.h>
#include <IM.Server.pb.h>
#include <string.h>
#include "leveldb/write_batch.h"
using namespace com::proto::msg;
using namespace com::proto::state;
using namespace com::proto::server;

namespace msg_server {
	static int sec_recv_pkt;
	static int total_recv_pkt;
	static const int db_index_offline_msg = 15;

	static int sec_send_pkt;
	int total_send_pkt;
	void MsgServer::check_msg(MsgServer* s) {
		//return;
		while (1) {
			sleep(2);
			s->check_send_msg();
		}
	}
	MsgServer::MsgServer() {
		m_cur_index_ = 0;
		m_lastTime = 0;
		m_pNode = CNode::getInstance();
		m_mq_fd = -1;
	}

	int MsgServer::sendConn(SPDUBase& base)
	{
		if (NodeMgr::getInstance()->sendNode(SID_CONN, base.node_id, base) == -1) {
			LOGE("send node[sid:%s,nid:%d]fail", sidName(SID_CONN), base.node_id);
		}
	}

	MsgServer * MsgServer::getInstance()
	{
		static MsgServer instance;
		return &instance;
	}

	int MsgServer::init()
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
		if (pipeline_clear_offline_msg_redis_.connect(redis_ip, redis_port, auth, db_index_offline_msg) != 0) {
			LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
			return -1;
		}
		m_chat_msg_process.init(ProcessClientMsg);

		//m_common_msg_process.init(ProcessClientMsg, 2);

		ack_time_ = MSG_ACK_TIME;
		std::thread check_msg_(check_msg, this);
		//std::thread check_msg_(std::bind(&MsgServer::check_send_msg,this));
		check_msg_.detach();
		// tcp阻塞连接，用于查询
		//  block_tcp_client_.Connect(query_ip_.c_str(), query_port_, false);
		//    m_route_works = new ThreadPool(2); //handler route msg;
		m_storage_msg_works = new ThreadPool(1);//save msg redis;
		std::string file = ConfigFileReader::getInstance()->ReadString("statistics");
		if (file != "") {
			m_statistics.init(file);
		}

		leveldb::Options options;
		options.create_if_missing = true;
		std::string dbpath = "testdb";
		leveldb::Status status = leveldb::DB::Open(options, dbpath, &m_db);
		if (!status.ok()) {
			LOGE("open db cache fail");
			return -1;
		}

		m_loop = getEventLoop();
		m_loop->init(1024);
		TcpService::init(m_loop);
		CreateTimer(50, Timer, this);
		m_pNode->init(this);
		std::string mq_ip = ConfigFileReader::getInstance()->ReadString(CONF_MGQUEUE_IP);
		short mq_port = ConfigFileReader::getInstance()->ReadInt(CONF_MGQUEUE_PORT);
		m_mq_fd = StartClient(mq_ip, mq_port);
		NodeMgr::getInstance()->init(m_loop, innerMsgCb, this);
		NodeMgr::getInstance()->setConnectionStateCb(connectionStateEvent, this);

	}

	void MsgServer::start() {
		m_chat_msg_process.start();
		sleep(1);
		LOGD("MsgServer listen on %s:%d", m_ip.c_str(), m_port);
		TcpService::StartServer(m_ip, m_port);
		PollStart();
	}

	extern int total_recv_pkt;
	extern int total_user_login;

	void MsgServer::connectionStateEvent(NodeConnectedState* state, void* arg)
	{
		MsgServer* server = reinterpret_cast<MsgServer*>(arg);
		if (!server) {
			return;
		}
		if (state->sid == SID_DISPATCH && state->state == SOCK_CONNECTED) {
			server->registCmd(state->sid, state->nid);
		}
	}

	void MsgServer::registCmd(int sid, int nid)
	{
		LOGD("registcmd %s", sidName(sid));
		RegisterCmdReq req;
		int cur_nid = ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
		int cur_sid = ConfigFileReader::getInstance()->ReadInt(CONF_SERVICE_TYPE);
		req.set_node_id(cur_nid);
		req.set_service_type(cur_sid);

		req.add_cmd_list(CID_CHAT_MSG_ACK);
		req.add_cmd_list(CID_PUSH_MSG_ACK);

		SPDUBase spdu;
		spdu.ResetPackBody(req, CID_REGISTER_CMD_REQ);
		NodeMgr::getInstance()->sendNode(sid, nid, spdu);
	}

	void MsgServer::OnRecv(int sockfd, PDUBase* base) {
		SPDUBase* spdu = dynamic_cast<SPDUBase*>(base);
		int cmd = spdu->command_id;

		switch (cmd) {
		case CID_PUSH_MSG:
			// case BULLETIN_NOTIFY:
		case CID_CHAT_BUDDY:
		case CID_CHAT_GROUP:
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

	void MsgServer::OnConn(int sockfd) {
		//LOGD("建立连接fd:%d", sockfd);
	}

	void MsgServer::OnDisconn(int sockfd) {
		if (sockfd == m_mq_fd) {
			m_mq_fd = -1;
		}
		m_pNode->setNodeDisconnect(sockfd);
		CloseFd(sockfd);
	}

	/* ................handler msg recving from dispatch.............*/
	int MsgServer::processInnerMsg(int sockfd, SPDUBase & base)
	{
		int cmd = base.command_id;
		int index = 0;
		switch (cmd) {
			//user msg
		case CID_CHAT_BUDDY:
		case CID_CHAT_MSG_ACK:
		case CID_USER_STAT_PUSH_REQ://state msg
		case CID_PUSH_MSG_ACK:
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

	int MsgServer::innerMsgCb(int sockfd, SPDUBase & base, void * arg)
	{
		MsgServer* server = reinterpret_cast<MsgServer*>(arg);
		if (server) {
			server->processInnerMsg(sockfd, base);
		}
	}

	void MsgServer::ProcessClientMsg(int sockfd, SPDUBase* base)
	{
		MsgServer* pInstance = MsgServer::getInstance();
		//check user whether onlines;
		std::string user_id(base->terminal_token, sizeof(base->terminal_token));
		switch (base->command_id) {
		case CID_CHAT_BUDDY:
		case CID_CHAT_GROUP:
		case CID_PUSH_MSG:
			pInstance->ProcessPushMsg(sockfd, *base);
			break;
		case CID_CHAT_MSG_ACK:
		case CID_PUSH_MSG_ACK:	
			pInstance->ProcessPushMsg_ack(sockfd, *base);
			break;
		case BULLETIN_NOTIFY:
			//pInstance->ProcessBulletin_broadcast(sockfd, *base);
			break;
		case CID_USER_STAT_PUSH_REQ:
			pInstance->ProcessUserstateBroadcast(*base);
			break;
		case LOCATIONSHARE:
			//pInstance->ProcessLocationShareContinue(sockfd, *base);
			break;
		case 0:
			pInstance->ack_timeout_handler(base->terminal_token);
			break;
		default:
			LOGE("unknow cmd:%d", base->command_id);
			break;
		}
		delete base;

	}

	int  MsgServer::saveChatMsg(SPDUBase&  base, uint64_t msg_id) {
		char* value;
		int len = base.OnPduPack(value);
		std::string data(value, len);
		delete[]value;
		leveldb::Status status = m_db->Put(leveldb::WriteOptions(), std::to_string(msg_id), data);
		if (!status.ok()) {
			LOGE("insert msg_id:%d to db cache fail", msg_id);
			return -1;
		}
		//LOGD("save  msg(%ld) lenght:%d",msg_id,data.length());
		return 0;
	}

	int  MsgServer::getChatMsg(SPDUBase&  base, uint64_t msg_id) {
		std::string value;
		leveldb::Status  status = m_db->Get(leveldb::ReadOptions(), std::to_string(msg_id), &value);
		if (!status.ok()) {
			LOGE("get msg_id:%d from db cache fail", msg_id);
			return -1;
		}
		// LOGD("get msg(%ld) lenght:%d",msg_id,value.length());
		base._OnPduParse(value.data(), value.length());
		return 0;
	}

	/*
	* 处理用户发送的IM消息
	* 注意：用户发送消息时只有自己的userid和对方的手机号，所以其他信息需要查询获得
	*/
	void MsgServer::ProcessPushMsg(int sockfd, SPDUBase&  base) {
		S2SPushMsg  push;
		Device_Type device_type = DeviceType_UNKNOWN;

		if (!push.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "IMChat_Personal包解析错误");
			ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
			return;
		}
		const auto& msg = push.msg();
		int cmd = msg.cmd_id();
		uint64_t msg_id = msg.msg_id();
		
		if (cmd == CID_CHAT_BUDDY || cmd == CID_CHAT_GROUP || cmd==CID_CHAT_MACHINE || cmd==CID_CHAT_CUSTOMER_SERVICES) {
			 ChatMsg  data ;
			 
			 if (!data.ParseFromString(msg.data())) {
				 LOGERROR(base.command_id, base.seq_id, "IMChat_Personal包解析错误");
				 ReplyChatResult(sockfd, base, ERRNO_CODE_DATA_SRAL);
				 return;
			 }


			const MsgData& msg = data.data();

			LOGD("recv msg_id[%lld] type(%d)", msg_id, msg.content_type());
			if (msg.content_type() == 0) {
				LOGD("content msg:%s", msg.msg_content().c_str());
			}
			ResetPackBody(base, data, cmd);
		}
		else {
			//save push instance
			ResetPackBody(base, msg, base.command_id);
		}
		
		saveChatMsg(base, msg_id);
	    auto  user_list = push.mutable_user_id();
		auto it = user_list->begin();
        int num=user_list->size();
		for (it; it != user_list->end(); ) {
			user_t* dest_user = m_user_cache.queryUser(*it);
			if (dest_user) {
				LOGI("user[user_id:%s]%s", dest_user->user_id.c_str(), dest_user->state());
				//if user offline ignore it
				if (dest_user->online== USER_STATE_ONLINE) {
					ProcessIMChat_fromRoute(base, msg_id, dest_user);
					it = user_list->erase(it);
					continue;
				}
				else {
					dest_user->has_offline_msg = true;
					if (dest_user->online == USER_STATE_LOGOUT) {
						it = user_list->erase(it);
						continue;
					}
				}
				
			}
			else {
				LOGE("query user[%s] fail",it->c_str());
			}
			++it;

		}

		if (cmd == CID_CHAT_BUDDY || cmd == CID_CHAT_GROUP) {
            if(user_list->size()>0){
			    base.ResetPackBody(push, cmd);
			    Send(m_mq_fd, base);
            }
		}
		std::lock_guard<std::recursive_mutex> lock(m_msg_count_mutex_);
		m_msg_count_.insert(std::pair<uint64_t, int>(msg_id, num));
	}

	bool MsgServer::DeleteMsgIdtoRedis(const std::string& user_id, uint64_t msg_id) {

		std::string id = std::to_string(msg_id);
		const char *command[3];
		size_t vlen[3];

		command[0] = "SREM";
		command[1] = user_id.c_str();
		command[2] = id.c_str();
		vlen[0] = 4;
		vlen[1] = user_id.length();
		vlen[2] = id.length();
		bool res = storage_offline_msg_redis_.query(command, sizeof(command) / sizeof(command[0]), vlen);
		if (!res) {
			LOGE("msgserver: delete msg fail");
		}
	}


	bool MsgServer::GetOfflineMsgIds(const std::string& user_id, std::list<std::string> &id_list) {

		const char *command[2];
		size_t vlen[2];

		command[0] = "SMEMBERS";
		command[1] = user_id.c_str();

		vlen[0] = 8;
		vlen[1] = user_id.length();

		return storage_offline_msg_redis_.query(command, sizeof(command) / sizeof(command[0]), vlen, id_list);

	}

	bool MsgServer::AppendDeleteMsgIdtoRedis(const std::string& user_id, uint64_t msg_id) {
		std::string id = std::to_string(msg_id);
		const char *command[3];
		size_t vlen[3];

		command[0] = "SREM";
		command[1] = user_id.c_str();
		command[2] = id.c_str();
		vlen[0] = 4;
		vlen[1] = user_id.length();
		vlen[2] = id.length();

		bool res = pipeline_clear_offline_msg_redis_.appendCommandArgv(command, sizeof(command) / sizeof(command[0]), vlen);
		if (!res) {
			LOGE("msgserver: add delete msg fail");
		}
	}

	bool MsgServer::ExecuteCmdRedis(int num) {
		int res = pipeline_clear_offline_msg_redis_.getReply(num);
		if (res != num) {
			LOGE("group: storage msg  fail,storage num:%d", res);
		}
	}

	void MsgServer::recordAckMsg(Ackmsg* msg) {
		std::lock_guard<std::recursive_mutex> lock(m_msg_clear_mutex_);
		m_ackmsg_list.push_back(msg);
	}

	//Deleting  most 100 msg from leveldb and redis every time
	void MsgServer::clearRecordAckMsg() {
		std::lock_guard<std::recursive_mutex> lock(m_msg_clear_mutex_);
		for (int i = 0; i < 5; i++) {
			int count = 0;
			leveldb::WriteBatch batch;
			auto it = m_ackmsg_list.begin();
			while (it != m_ackmsg_list.end()) {
				Ackmsg* msg = *it;
				AppendDeleteMsgIdtoRedis((*it)->user_id, (*it)->msg_id);
				++count;
				//remove msg from leveldb
				{
					std::lock_guard<std::recursive_mutex> lock(m_msg_count_mutex_);
					auto mit = m_msg_count_.find(msg->msg_id);
					if (mit != m_msg_count_.end()) {
						LOGD("msg_id[%lld] count %d", msg->msg_id, mit->second);
						--mit->second;
						if (0 == mit->second) {
							batch.Delete(std::to_string(msg->msg_id));
							m_msg_count_.erase(mit);

						}
					}
				}

				it = m_ackmsg_list.erase(it);
				delete msg;
				if (count == 100) {
					break;
				}
			}
			if (count>0) {
				ExecuteCmdRedis(count);
				leveldb::Status  status = m_db->Write(leveldb::WriteOptions(), &batch);
				if (count < 100) {
					break;
				}
			}
		}

	}

	void MsgServer::ProcessPushMsg_ack(int sockfd, SPDUBase & base) {
		ChatMsg_Ack ack;
		if (!ack.ParseFromArray(base.body.get(), base.length)) {
			LOGERROR(base.command_id, base.seq_id, "Broadcast parse fail");
			return;
		}
		const std::string&  user_id = ack.user_id();
		user_t* user = m_user_cache.findUser(user_id);
		if (!user) {
			LOGE("not find ackmsg user(user_id:%s)", user_id.c_str());
			return;
		}
		long long cur_ms = get_mstime();
		user->ack_time = cur_ms;
		uint64_t msg_id = ack.msg_id();
		LOGI("recv user[id:%s ack msg:%ld]", user_id.c_str(), msg_id);

		//	std::lock_guard<std::recursive_mutex> lock1(m_send_msg_mutex_);
		//	CAutoRWLock lock(&m_rwlock_, 'w');
		auto it = m_send_msg_map_.find(user_id);
		if (it != m_send_msg_map_.end()) {//last msg is waiting for ack
			if (!it->second.empty()) {
				Ackmsg* ackmsg = it->second.front();
				if (ackmsg->msg_id != msg_id && ackmsg->msg_id != 0) {
					LOGE("user_id(%s) rsp error ack msg_id(%ld)", user_id.c_str(), msg_id);//because of user having recving the pkt even if msg_id not true. we continue send it next msg due to the using online
					return;
				}
				LOGI("msg_id(%ld)  ack latency time %d ms,send latency time:%d ms", msg_id, cur_ms - user->send_time, cur_ms - ackmsg->ms);
				//delete ackmsg;
				it->second.pop_front();
				delete_ack_msg(user_id, msg_id);
				user->send_pending = 0;
			//	DeleteMsgIdtoRedis(user_id, std::to_string(msg_id));
				recordAckMsg(ackmsg);
				if (!it->second.empty()) {
					ackmsg = it->second.front();
					//            LOGD("begin send msg_id(%ld)", ackmsg->msg_id);
					SPDUBase spdu;
					if (getChatMsg(spdu, ackmsg->msg_id) == 0) {
						memcpy(spdu.terminal_token, user_id.c_str(), sizeof(spdu.terminal_token));
						spdu.sockfd = user->fd;
						spdu.node_id = user->nid;
						sendConn(spdu);//ignore send result,if fail 
						user->send_pending = 1;
						user->send_time = cur_ms;
						record_waiting_ackmsg(user_id, ackmsg->msg_id);
					}
					else {
						recordAckMsg(ackmsg);
						delete ackmsg;
						it->second.pop_front();
					}
				}
			}
		}

	}

	void MsgServer::ProcessUserstateBroadcast(SPDUBase & _base)
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
			user->online = state_broadcast.state();
			user->version = state_broadcast.version();
			user->device_type = state_broadcast.device_type();
		}
		else {
			LOGE("state broadcast error");
			return;
		}
		
		LOGD("state broadcast,user[user_id:%s nid:%d,fd:%d] %s", user->user_id.c_str(), user->nid, user->fd, user->state());
		if (user->online== USER_STATE_ONLINE && user->has_offline_msg) {
			ConsumeHistoryMessage(user_id);
		}
		if (user->online== USER_STATE_ONLINE) {
			user->online_time = state_broadcast.time();
		}
	}

	void MsgServer::ResetPackBody(SPDUBase &_pack, const google::protobuf::Message &_msg, int _commandid) {
		std::shared_ptr<char> body(new char[_msg.ByteSize()], carray_deleter);
		_msg.SerializeToArray(body.get(), _msg.ByteSize());
		_pack.body = body;
		_pack.length = _msg.ByteSize();
		_pack.command_id = _commandid;
	}

	void MsgServer::ProcessIMChat_fromRoute(SPDUBase &base, uint64_t msg_id, user_t* user) {

		memcpy(base.terminal_token, user->user_id.c_str(), sizeof(base.terminal_token));
		base.node_id = user->nid;
		base.sockfd = user->fd;
		need_send_msg(user, base, msg_id);
	}

	void MsgServer::reportOnliners()
	{
		//printf("total recv %d pkts,every recv %d pkt, total send:%d ,every sec send:%d \n", total_recv_pkt, total_recv_pkt - sec_recv_pkt, total_send_pkt, total_send_pkt - sec_send_pkt);
		sec_recv_pkt = total_recv_pkt;
		sec_send_pkt = total_send_pkt;
	}

	void MsgServer::Timer(int fd, short mask, void * privdata)
	{
		MsgServer* server = reinterpret_cast<MsgServer*>(privdata);
		if (server) {
			//server->reportOnliners();
			server->clearRecordAckMsg();
			server->cron();
			return;
		}
	}

	void MsgServer::cron()
	{
		if (m_mq_fd == -1) {
			std::string mq_ip = ConfigFileReader::getInstance()->ReadString(CONF_MGQUEUE_IP);
			short mq_port = ConfigFileReader::getInstance()->ReadInt(CONF_MGQUEUE_PORT);
			if (mq_ip != "") {
				m_mq_fd = StartClient(mq_ip, mq_port);
				if (m_mq_fd == -1) {
					LOGE("connect mq(addr: %s:%d) fail", mq_ip.c_str(), mq_port);
				}
			}
		}
	}

	void MsgServer::ConsumeHistoryMessage(const std::string& user_id) {

		user_t* user = m_user_cache.findUser(user_id);
		if (!user) {
			LOGE("not find user_id(%s)", user_id.c_str());
			return;
		}
		SPDUBase base;
	
		std::list<std::string> msg_id_list;
		//msg type: offline chat msg  and user timeout recv msg
		if (GetOfflineMsgIds(user_id, msg_id_list)) {

			LOGD("user[user_id:%s] have %d offline chatmsg", user_id.c_str(), msg_id_list.size());

			for (auto item = msg_id_list.begin(); item != msg_id_list.end(); item++) {
				uint64_t msg_id = atoll(item->c_str());
				if (getChatMsg(base, msg_id) == 0) {
					//	ProcessIMChat_fromRoute(base);
					/*IMChat_Personal_Notify im_notify;
					if (!im_notify.ParseFromArray(base.body.get(), base.length)) {
					LOGERROR(base.command_id, base.seq_id, "ConsumeHistoryMessage包解析错误");
					return;
					}
					const IMChat_Personal& im = im_notify.imchat();*/
					memcpy(base.terminal_token, user_id.c_str(), sizeof(base.terminal_token));
					base.sockfd = user->fd;
					base.node_id = user->nid;
					need_send_msg(user, base, msg_id);
				}
				else {
					DeleteMsgIdtoRedis(user_id, msg_id);
				}
			}

		}

#if 0
		std::list<std::string> channel_list;
		if (redis_client.GetBroadcastOfflineIMList(_userid, channel_list)) {
			LOGD("user_id(%d) have %d broadcast msg ", _userid, channel_list.size());
			for (auto it = channel_list.begin(); it != channel_list.end(); ++it) {
				LOGD("channel:%s", (*it).c_str());
				std::string chat_msg = redis_client.GetBroadcastMsg(*it);
				if (chat_msg == "") {
					LOGD("not find channel:%s msg", (*it).c_str());
					return;
				}
				std::string tmp_msg_id = (*it).substr((*it).find(":") + 1);
				uint32_t msg_id = atoi(tmp_msg_id.c_str());
				Json::Reader reader;
				Json::Value root;
				if (!reader.parse(chat_msg, root)) {
					LOGE(" parse fail");
					return;
				}
				int cmd = atoi(root["CommandId"].asString().c_str());
				::google::protobuf::Message* notify;
				ChatMsg im;
				IMChat_Personal_Notify im_notify;
				Bulletin_Notify  bulletin_notify;
				if (cmd == IMCHAT_PERSONAL) {
					notify = &im_notify;
					im.set_msg_id(msg_id);
					im.set_src_usr_id(atoi(root["SvcUsrId"].asString().c_str()));
					im.set_src_phone(root["SvcPhone"].asString());
					im.set_content_type(atoi(root["ContentType"].asString().c_str()));
					im.set_command_id(atoi(root["CommandId"].asString().c_str()));
					im.set_body(root["Body"].asString());
					im.set_target_user_type(USER_TYPE_PERSONAL);
					im.set_timestamp(root["Timestamp"].asInt());
					LOGD("broadcast offline chat msg[ msg_id:%s] to user[ph:%s]", (*it).c_str(), user->phone);

					im.set_target_user_id(_userid);
					im.set_target_phone(user->phone);
					im_notify.set_allocated_imchat(&im);
				}
				else if (cmd == BULLETIN) {
					LOGD("broadcast offline bulletin msg [ msg_id:%s ] to user[ph:%s]", (*it).c_str(), user->phone);
					Bulletin bul;
					notify = &bulletin_notify;
					bul.set_bulletin_id(msg_id);
					bul.set_content(root["Body"].asString());
					bul.set_publish_time(root["Timestamp"].asInt());
					bul.set_publisher_id(atoi(root["SvcUsrId"].asString().c_str()));
					bul.set_bulletin_type((Bulletin_Type)atoi(root["ContentType"].asString().c_str()));
					bul.set_publisher_phone(root["SvcPhone"].asString());
					auto it = bulletin_notify.add_bulletins();
					it->CopyFrom(bul);
				}

				std::shared_ptr<char> body(new char[notify->ByteSize()], carray_deleter);
				notify->SerializeToArray(body.get(), notify->ByteSize());

				base.body = body;
				base.length = notify->ByteSize();

				if (cmd == BULLETIN) {
					base.command_id = BULLETIN_NOTIFY;
				}
				else {
					base.command_id = atoi(root["CommandId"].asString().c_str());
				}
				if (user->version == VERSION_0 || user->version == VERSION_1 && cmd == BULLETIN) {
					sendConn(base);
				}
				else {
					need_send_msg(user, base, msg_id);
				}

				if (cmd == IMCHAT_PERSONAL) {
					im_notify.release_imchat();
				}
			}
		}
#endif
		user->has_offline_msg = false;

	}

	uint64_t MsgServer::getMsgId()
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

	bool MsgServer::need_send_msg(user_t* user, SPDUBase& base, uint64_t msg_id)
	{
		std::string& user_id = user->user_id;
		Ackmsg* ack_msg = new Ackmsg(user_id,msg_id);
		{
			//std::lock_guard<std::recursive_mutex> lock1(m_send_msg_mutex_);
			//	CAutoRWLock lock(&m_rwlock_, 'w');
			//first push the msg to the queue
			auto it = m_send_msg_map_.find(user_id);
			if (it == m_send_msg_map_.end()) {
				std::list<Ackmsg*> ml;
				ml.push_back(ack_msg);
				m_send_msg_map_.insert(std::pair<std::string, std::list<Ackmsg*>>(user_id, ml));
			}
			else {
				it->second.push_back(ack_msg);
			}

		}
		//second decide  whether send the msg
		if (user->send_pending == 1) {
			return false;
		}

		user->send_pending = 1;
		user->send_time = get_mstime();

		sendConn(base);
		record_waiting_ackmsg(user_id, msg_id);//msg_id not use;
		return true;
	}

	void MsgServer::record_waiting_ackmsg(const std::string& user_id, uint64_t msg_id) {
		//std::lock_guard<std::recursive_mutex> lock(m_ack_msg_mutex_);
		time_t t = time(0);
		//LOGD("add ack msg_id(%ld) time:%d",msg_id,t);
		m_ack_msg_map_.push(user_id, t);
	}

	void MsgServer::delete_ack_msg(const std::string& user_id, uint64_t msg_id)
	{
		std::lock_guard<std::recursive_mutex> lock(m_ack_msg_mutex_);
		auto it = m_ack_msg_map_.find(user_id);
		if (it != m_ack_msg_map_.end()) {
			m_ack_msg_map_.erase(it);
		}
		else {
			LOGE("not find user_id(%s)", user_id.c_str());
		}
	}

	void MsgServer::check_send_msg()
	{
		time_t now = time(NULL);
		std::vector<std::string> offline_users;
		{
			std::lock_guard<std::recursive_mutex> lock(m_ack_msg_mutex_);
			while (!m_ack_msg_map_.empty()) {
				auto it = m_ack_msg_map_.back();
				if (it.second + MSG_ACK_TIME < now) {//timeout recv ack
					offline_users.push_back(it.first);
					m_ack_msg_map_.pop();
					LOGE("timeout recv ack user_id(%s),prev(%d),now(%d)", it.first.c_str(), it.second, now);
					continue;
				}
				break;
			}
		}
		//std::lock_guard<std::recursive_mutex> lock1(m_send_msg_mutex_);
		//CAutoRWLock lock(&m_rwlock_, 'w');
		for (auto it = offline_users.begin(); it != offline_users.end(); it++) {
			SPDUBase* pdu = new SPDUBase;
			pdu->command_id = 0;
			// pdu->terminal_token = *it;
			memcpy(pdu->terminal_token, it->c_str(), sizeof(pdu->terminal_token));
			m_chat_msg_process.addJob(0, pdu, 1);

		}
	}

	void MsgServer::ack_timeout_handler(const std::string& user_id)
	{
		LOGD("handler ack timeout");
		user_t* user = m_user_cache.findUser(user_id);
		if (user) {
			int cur_time = time(0);
			if (cur_time - user->online_time < 2 * MSG_ACK_TIME && user->send_pending == 1) {// user online ,but not recv ack send again
				auto it = m_send_msg_map_.find(user_id);
				if (it != m_send_msg_map_.end()) {//last msg is waiting for ack
					if (!it->second.empty()) {

						Ackmsg* ackmsg = it->second.front();
						LOGD("try again send msg_id(%ld)", ackmsg->msg_id);

						SPDUBase spdu;
						if (getChatMsg(spdu, ackmsg->msg_id) == 0) {
							memcpy(spdu.terminal_token, user_id.c_str(), sizeof(spdu.terminal_token));
							spdu.sockfd = user->fd;
							spdu.node_id = user->nid;
							user->send_pending = 1;
							user->send_time = get_mstime();
							sendConn(spdu);//ignore send result,if fail 

							record_waiting_ackmsg(user_id, ackmsg->msg_id);
						}

					}
				}
			}
			else {//delete willing send msg
				auto it = m_send_msg_map_.find(user_id);
				if (it != m_send_msg_map_.end()) {//last msg is waiting for ack

					std::list<Ackmsg*>& msg_list = it->second;
					for (auto msg_it = msg_list.begin(); msg_it != msg_list.end(); msg_it++) {
						//	SaveOfflineMsg(*it, (*msg_it)->pdu);//save timeout user msg
						delete *msg_it;

					}
					m_send_msg_map_.erase(it);
				}
				LOGD("timeout recv ack user[id:%s],set status offline", user_id.c_str());
				user->online = USER_STATE_OFFLINE;
				user->has_offline_msg = true;
				user->send_pending = 0;
			}
		}
		else {
			LOGE("not find user_id(%s)", user_id.c_str());
		}

	}


	/*
	* 小的封装函数，只组合IM消息结果的包，发送给用户．
	*/
	void MsgServer::ReplyChatResult(int sockfd, SPDUBase &_pack, ERRNO_CODE _code, bool is_target_online, uint64_t msg_id) {
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
		while (it != user_list_.end()) {
			delete *it;
			it = user_list_.erase(it);
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

	void LocationShareTask::notifyUsers(MsgServer* msg_server, Fnotify cb, const google::protobuf::Message &_msg, int cmd)
	{
		auto it = user_list_.begin();
		for (it; it != user_list_.end(); it++) {
			if ((*it)->answer_) {
				(msg_server->*cb)((*it)->user_id_, (*it)->phone_, _msg, cmd);
			}
		}
	}
}
