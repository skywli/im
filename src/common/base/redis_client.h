#ifndef _REDIS_CLIENT_H
#define _REDIS_CLIENT_H

#include <string>
#include <map>
#include <mutex>
#include "base/hredis_base.h"
#include <pthread.h>
#define CZY_ORG_KEY          "czy_us_9959f117-df60-4d1b-a354-776c20ffb8c7"
// redis key的过期时间(单位秒，默认2个月)
#define REDIS_KEY_EXPIRE_DEFAULT 5184000

class RedisClient:public HRedisBase {
public:
    RedisClient();
    ~RedisClient();

    void Init_Pool(std::string _ip, int _port, std::string _auth, int _num=1);
	redisContext * ConnectRedis(std::string & ip, int port,std::string& auth);
	redisContext * getConnect();
//	redisContext* Connect(std::string& ip, int port);
    void SetKeysExpire(int _expire);

	

public:
	bool IsCZYUser(const std::string & user_id);
	bool HasUserInfo(const std::string & user_id);
    bool InsertIMtoRedis(const std::string& _imjson);
    bool InsertIMPushtoRedis(const std::string& _imjson);
    bool InsertUserSessionToRedis(int _userid, std::string _session);
	bool InsertOfflineIMtoRedis(const std::string & user_id, const char * _encode_im);

	bool InsertMsgIdtoRedis(const std::string & user_id, const std::string & id);

	bool DeleteMsgIdtoRedis(const std::string & user_id, const std::string & id);
 
	bool GetOfflineMsgIds(const std::string & user_id, std::list<std::string>& _session_list);

	bool InsertBroadcastOfflineIMtoRedis(const std::string & _userid, const std::string & _channel);
	std::string GetBroadcastMsg(const std::string&  _key);
    bool GetUserSessionList(int _userid, std::list<std::string> &_session_list);
    bool GetOfflineIMList(int _userid, std::list<std::string> &_encode_imlist);

	bool GetBroadcastOfflineIMList(int  _userid, std::list<std::string> &_out_list);
    bool GetIMUserList(std::list<std::string> &_keys);

    std::string GetUserId(std::string _phone);
    std::string GetSessionId(std::string _phone);
	bool GetUserInfo(std::string _phone, std::list<std::string> &_out_list);

	std::string GetHashValue(const std::string& key, const std::string& field);
	bool GetChannelUserList(const std::string& key, int start,int stop,std::list<std::string>& out_list);

	bool getUserState(const std::string & user_id, std::string & _value);

	bool query(const char * command[], int num, size_t * vlen, std::string & value);
	
	//user state
	bool getUserState(int user_id,std::string& _value);
	bool setUserState(const std::string & user_id, const char * _value);
	bool deleteUserState(const std::string & user_id);

private:
 //   std::list<redisContext*> free_connection_list_;
	std::map<pthread_t, redisContext*> m_redis_conns;
    std::recursive_mutex list_mutex_;
	int expire_ = REDIS_KEY_EXPIRE_DEFAULT;
  

};

#endif
