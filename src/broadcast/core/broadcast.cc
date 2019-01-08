#include "broadcast.h"

#include "IM.Route.pb.h"
#include "IM.Transfer.pb.h"
#include "logic_util.h"
#include "IM.Chat.pb.h"
#include <log_util.h>
#include <config_file_reader.h>
#include "IM.Bulletin.pb.h"
#include <json/json.h>
#include <unistd.h>
#include <deleter.h>
#include <node_mgr.h>
using namespace com::proto::basic;
using namespace com::proto::chat;
using namespace com::proto::route;
using namespace com::proto::bulletin;
using namespace com::proto::transfer;

BroadcastServer* pInstance;
std::string notify_url;
int httpClient(const char* url);
void notify(const char* msg_id) {
	char url[256] = { 0 };
	sprintf(url, "%s?msgId=%s&v=4", notify_url.c_str(), msg_id);
	httpClient(url);

}

BroadcastServer::BroadcastServer()
{
	pInstance = this;
}

BroadcastServer::~BroadcastServer()
{
}

void BroadcastServer::OnRecv(int _sockfd, PDUBase * _base)
{
}

void BroadcastServer::OnConn(int _sockfd)
{
}

void BroadcastServer::OnDisconn(int _sockfd)
{
}

void BroadcastServer::SyncUserData(std::list<std::string>& _keys) {
	if (_keys.size() == 0) return;
	
	for (auto it = _keys.begin(); it != _keys.end(); it++) {
		std::string phone = LogicUtil::get_phone(*it);
		if (m_user_map.find(phone) == m_user_map.end()) {
			std::list<std::string> userinfo;
			if (redis_client.GetUserInfo(phone, userinfo)) {
				User*  user = new   User;
				for (auto it = userinfo.begin(); it != userinfo.end(); it++) {
					if (*it == "uid") {
						user->id = atoi((*(++it)).c_str());
					}
					else if (*it == "type") {
						user->utype = atoi((*(++it)).c_str());
					}
					else if (*it == "sdkChannel") {
						user->sdkChannel = atoi((*(++it)).c_str());
					}
					else if (*it == "appChannel") {
						user->appChannel = atoi((*(++it)).c_str());
					}
				}
				m_user_map[phone] = user;
			}
		}
	}
	
}

void BroadcastServer::InitIMUserList() {
	std::list<std::string> keys;

	redis_client.GetIMUserList(keys);
	LOGI("total users:%d", keys.size());
	struct timeval start, stop;
	gettimeofday(&start, NULL);
	SyncUserData(keys);
	gettimeofday(&stop, NULL);
	int sec = stop.tv_sec - start.tv_sec;
	int usec = stop.tv_sec - start.tv_sec;
	if (usec < 0) {
		usec += 1000;
		sec -= 1;
	}
	LOGD("sync success users:%d  %d ms", m_user_map.size(), sec * 1000 + usec / 1000);
}

int BroadcastServer::init()
{
	m_ip = ConfigFileReader::getInstance()->ReadString(CONF_LISTEN_IP);
	m_port = ConfigFileReader::getInstance()->ReadInt(CONF_LISTEN_PORT);
	if (m_ip == "" || m_port == 0) {
		LOGD("not config ip or port");
		return -1;
	}
	notify_url = ConfigFileReader::getInstance()->ReadString("notify_url");
	LOGD("notify_url:%s", notify_url.c_str());
	std::string redis_ip = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_IP);
	short redis_port = ConfigFileReader::getInstance()->ReadInt(CONF_REDIS_PORT);
	std::string auth = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_AUTH);
	LOGD("redis ip:%s,port:%d", redis_ip.c_str(), redis_port);

	redis_client.Init_Pool(redis_ip, redis_port, auth);
	InitIMUserList();

	//start http server
	std::string out_ip = ConfigFileReader::getInstance()->ReadString(CONF_CONN_OUT_IP);
	short out_port = ConfigFileReader::getInstance()->ReadInt(CONF_CONN_OUT_PORT);
	if (out_ip == "" || out_port == 0) {
		LOGD("not config http ip or port");
		return -1;
	}
	m_loop = getEventLoop();
	m_loop->init(1024);
	TcpService::init(m_loop);
	 NodeMgr::getInstance()->init(this, m_loop, NULL, NULL);
	return HttpServer::init(out_ip, out_port);
		
}

int BroadcastServer::start()
{
	HttpServer::start();
	LOGD("BroadcastServer server listen on %s:%d", m_ip.c_str(), m_port);
	if (TcpService::StartServer(m_ip, m_port) == -1) {
		LOGE("listen [ip:%s,port:%d]fail", m_ip.c_str(), m_port);
		return -1;
	}
	PollStart();
	return 0;
}

int BroadcastServer::HandlerBroadcastMsg(int sendType, std::string& sendTypeContent, int cmd,const std::string& msg_id, const char* data, int len) {
	google_list_u32 user_list;
	int num = 0;
	if (ST_PHONE == sendType) {
		std::list<std::string> phone_list;
		SplitString(sendTypeContent, phone_list);
		for (auto it = phone_list.begin(); it != phone_list.end(); it++) {
			int user_id = 0;
			auto uit = m_user_map.find(*it);
			LOGD("send phone:%s", (*it).c_str());
			if (uit == m_user_map.end()) {
				std::list<std::string> userinfo;
				if (redis_client.GetUserInfo((*it), userinfo)) {
					User*  user = new   User;
					for (auto it = userinfo.begin(); it != userinfo.end(); it++) {
						if (*it == "uid") {
							user->id = atoi((*(++it)).c_str());
							user_id = user->id;
						}
						else if (*it == "type") {
							user->utype = atoi((*(++it)).c_str());
						}
						else if (*it == "sdkChannel") {
							user->sdkChannel = atoi((*(++it)).c_str());
						}
						else if (*it == "appChannel") {
							user->appChannel = atoi((*(++it)).c_str());
						}
					}
					if (!userinfo.empty()) {
						m_user_map[*it] = user;
					}
					else {
						LOGE("invalied phone:%s", (*it).c_str());
					}
				}
			}//end uit
			else {
				user_id = uit->second->id;
			}
			if (user_id) {
				++num;
				user_list.Add(user_id);
				if (!(num % 2000)) { //send when num=2000*n
					BroadcastMsg(user_list, msg_id, data, len, 0, cmd);
					user_list.Clear();
				}
			}

		}

		if (num % 2000) {
			BroadcastMsg(user_list, msg_id, data, len, 0, cmd);
		}
	}
	else {
		if (ST_ALL_USER == sendType) {
			InitIMUserList();
		}
		for (auto it = m_user_map.begin(); it != m_user_map.end(); it++) {
			if (ST_ALL_USER == sendType) {
				++num;
				user_list.Add(it->second->id);
			}
			else if (ST_USER_TYPE == sendType) {

			}
			else if (ST_APP_CHANNEL == sendType) {

			}
			else if (ST_SDK_CHANNEL == sendType) {
			}
			else if (ST_PHONE == sendType) {
			}
			else {
				LOGD("unknow sendtype(%d)", sendType);
				return -1;
			}
			if (!(num % 2000)) {
				BroadcastMsg(user_list, msg_id, data, len, 0, cmd);
				user_list.Clear();
			}
			if (!(num % 10000)) {
				sleep(1);
			}
		}
		if (num % 2000) {
			BroadcastMsg(user_list, msg_id, data, len, 0, cmd);
		}
	}
}
int BroadcastServer::IMChatBroadcast(Json::Value& root,std::string& msg_id) {
	ChatMsg  im;
	IMChat_Personal_Notify notify;
	int src_user_id = atoi(root["SvcUsrId"].asString().c_str());
	im.set_msg_id(atoi(msg_id.c_str()));
	im.set_src_usr_id(src_user_id);
	im.set_src_phone(root["SvcPhone"].asString());
	im.set_content_type(atoi(root["ContentType"].asString().c_str()));
	im.set_command_id(atoi(root["CommandId"].asString().c_str()));
	im.set_body(root["Body"].asString());
	im.set_target_user_type(USER_TYPE_PERSONAL);
	im.set_timestamp(root["Timestamp"].asInt());
	LOGD("broadcast imchat  msg body:%s", root["Body"].asString().c_str());
	notify.set_allocated_imchat(&im);

	std::shared_ptr<char> body(new char[notify.ByteSize()], carray_deleter);
	notify.SerializeToArray(body.get(), notify.ByteSize());
	int sendType = atoi(root["SendType"].asString().c_str());
	std::string sendTypeContent = root["SendTypeContent"].asString();
	//send all user
	HandlerBroadcastMsg(sendType, sendTypeContent, IMCHAT_PERSONAL_NOTIFY, msg_id, body.get(), notify.ByteSize());
	notify.release_imchat();
}


int BroadcastServer::BulltinBroadcast(Json::Value& root, std::string& msg_id) {
    LOGD("bulletin msg");
	Bulletin bul;
	Bulletin_Notify  notify;
	bul.set_bulletin_id(atoi(msg_id.c_str()));
	bul.set_content(root["Body"].asString());
	bul.set_publish_time(root["Timestamp"].asInt());
	bul.set_publisher_id(atoi(root["SvcUsrId"].asString().c_str()));
	bul.set_bulletin_type((Bulletin_Type)atoi(root["ContentType"].asString().c_str()));
	bul.set_publisher_phone(root["SvcPhone"].asString());
    auto it=notify.add_bulletins();
    it->CopyFrom(bul);
	std::shared_ptr<char> body(new char[notify.ByteSize()], carray_deleter);
	notify.SerializeToArray(body.get(), notify.ByteSize());
	int sendType = atoi(root["SendType"].asString().c_str());
 
	std::string sendTypeContent = root["SendTypeContent"].asString();
	HandlerBroadcastMsg(sendType, sendTypeContent, BULLETIN_NOTIFY, msg_id, body.get(), notify.ByteSize());

}

int BroadcastServer::ProcessMsg(std::string  msg_id)
{
	std::string channel_msg = "channel_msg:"+ msg_id;

	std::string chat_msg = redis_client.GetBroadcastMsg(channel_msg);
	printf("broadcast msg:%s\n", chat_msg.c_str());
	if (chat_msg == "") {
		return -1;
	}

	Json::Reader reader;
	Json::Value root;
	if (!reader.parse(chat_msg.c_str(), root)) {
		LOGE(" msg_id : %s parse fail", chat_msg.c_str());
		return -1;
	}
	int cmd = atoi(root["CommandId"].asString().c_str());
	int exe_time = root["Timestamp"].asInt();
	int cur_time = time(0);
	if (exe_time > cur_time) {
		char* clientData = new char[msg_id.length()+1];
		memcpy(clientData, msg_id.c_str(), msg_id.length());
        clientData[msg_id.length()]='\0';
		createTimeEvent((exe_time -cur_time)*1000, _ProcessMsg, clientData);
        LOGD("send msg_id:%s after %d secs",msg_id.c_str(),exe_time-cur_time);
		return 0;
	}
	notify(msg_id.c_str());
	if (cmd == IMCHAT_PERSONAL) {
		IMChatBroadcast(root,msg_id);
	}
	else if (cmd == BULLETIN) {
		BulltinBroadcast(root, msg_id);
	}
	else {
		LOGD("unknow cmd(%d)", cmd);
	}
	return 0;

}

int BroadcastServer::BroadcastMsg(google_list_u32 & user_list,  const std::string & msg_id, const char* data, int len,int expire,int cmd)
{
	TransferBroadcastNotify broadcast;

	auto users = broadcast.mutable_user_id_list();
	users->CopyFrom(user_list);

	broadcast.set_msg_id(msg_id);
	broadcast.set_body(data,len);
	broadcast.set_expire(expire);

	std::shared_ptr<char> body(new char[broadcast.ByteSize()], carray_deleter);
	broadcast.SerializeToArray(body.get(), broadcast.ByteSize());
	

	SPDUBase pack;
	
	pack.body = body;
	pack.command_id =  cmd;
	pack.length = broadcast.ByteSize();
	pack.seq_id = 0;
	LOGD("broadcast msg");
	if (NodeMgr::getInstance()->sendRandomSid(SID_MSG, pack) == -1) {
		LOGE("send broadcast msg fail");
	}
	return 0;
}

int BroadcastServer::request_handler(evhttp_request * req)
{
	Broadcast_Notify notify;
	const char* uri = evhttp_request_get_uri(req);
	//	logd("Received a %s request for %s\nHeaders:", cmdtype, uri);
	char* decode_uri = evhttp_decode_uri(uri);
	LOGD("decode uri:%s", decode_uri);
	struct evkeyvalq params;
	evhttp_parse_query(decode_uri, &params);
	const char* reason = "success";
	const char* msg_id = evhttp_find_header(&params, "msg_id");
	bool res = true;
	if (msg_id == NULL) {
		LOGE("not find msg id");
		reason = "not find msg_id";
		res = false;
	}

	struct evbuffer* evb = evbuffer_new();
	evbuffer_add_printf(evb, "{ \"s\": \"1\",\"m\":\"%s\" }", reason);
	evhttp_send_reply(req, 200, "OK", evb);
    evbuffer_free(evb);
	if (res) {
		LOGD("broadcat msg_id:%s", msg_id);
		ProcessMsg(msg_id);
	}
}

int BroadcastServer::SplitString(std::string & src, std::list<std::string>& str_list)
{
	auto start = src.begin();
	auto it = src.begin();
	for (; it != src.end(); it++) {
        if(*it==' '){
            start++;
            continue;
        }
		if (*it == ',') {
			str_list.push_back(std::string(start, it));
			start = it + 1;
		}
	}
	if (!src.empty()) {
		str_list.push_back(std::string(start, it ));
	}
}

void BroadcastServer::_ProcessMsg(int fd,short mask,void* privdata)
{
	LOGD("timer send msg_id:%s",(char*)privdata);
	notify((char*)privdata);
	pInstance->ProcessMsg((char*)privdata);
	delete[] (char*)privdata;
	return ;
}

int BroadcastServer::GetUser(int sendType, int content, google_list_u32 & user_list)
{
}
