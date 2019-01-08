#ifndef _BRAODCAST_H_
#define _BRAODCAST_H_
#include <httpServer.h>
#include <transfer_client.h>
#include <redis_client.h>
#include <json/json.h>
#include <tcp_service.h>
typedef ::google::protobuf::RepeatedField< ::google::protobuf::int32 >  google_list_u32;
enum SendType {
	ST_ALL_USER=0,
	ST_USER_TYPE,
	ST_APP_CHANNEL,
	ST_SDK_CHANNEL,
	ST_PHONE
};
struct User {
	int            id;
	short          utype;
	short          appChannel;
	short          sdkChannel;

};
class BroadcastServer :public HttpServer,TcpService{
public:
	BroadcastServer();
	~BroadcastServer();
	virtual void OnRecv(int _sockfd, PDUBase* _base);
	virtual void OnConn(int _sockfd);
	virtual void OnDisconn(int _sockfd);
	void SyncUserData(std::list<std::string>& _keys);
	void InitIMUserList();
	int ReSyncUserData();
	int init();
	int start();
	int HandlerBroadcastMsg(int sendType, std::string & sendTypeContent, int cmd, const std::string & msg_id, const char * data, int len);

	int IMChatBroadcast(Json::Value & root, std::string & msg_id);

	int BulltinBroadcast(Json::Value & root, std::string & msg_id);

	int ProcessMsg(std::string  msg_id);

	int BroadcastMsg(google_list_u32 & user_list, const std::string & msg_id, const char* data, int len, int expire, int cmd);
	
	int request_handler(struct evhttp_request *req);

private:
	int GetUser(int sendType,int content, google_list_u32& user_list);
	int  SplitString(std::string& st, std::list<std::string>& str_list);
	static void _ProcessMsg(int fd, short mask, void * privdata);

private:
	TransferClient              transfer_client_;
	RedisClient                  redis_client;

	std::map<std::string, User*>   m_user_map;
	std::string                    m_ip;
	short                          m_port;
	SdEventLoop*                        m_loop;
};

#endif
