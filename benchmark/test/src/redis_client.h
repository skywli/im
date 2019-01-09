#ifndef _REDIS_CLIENT_H
#define _REDIS_CLIENT_H

#include <string>
#include <map>
#include <mutex>
#include "hredis_base.h"
#include <pthread.h>
#include <vector>
#include <set>
// redis key的过期时间(单位秒，默认2个月)
#define REDIS_KEY_EXPIRE_DEFAULT 5184000

class RedisClient:public HRedisBase {
public:
    RedisClient();
    ~RedisClient();

    void UnInit_Pool();
    void Init_Pool(std::string _ip, int _port, std::string _auth, int _num);
	redisContext * ConnectRedis(std::string & ip, int port,std::string& auth);
	redisContext * getConnect();
//	redisContext* Connect(std::string& ip, int port);
    void SetKeysExpire(int _expire);

public:
	bool GetIMUser(std::vector<std::string>& value);
	bool IsHuxinUser(std::string _phone);
    bool InsertIMtoRedis(std::string _imjson);
    bool InsertIMPushtoRedis(std::string _imjson);
    bool InsertUserSessionToRedis(int _userid, std::string _session);
    bool InsertOfflineIMtoRedis(int _userid, const char *_encode_im);
    bool GetUserSessionList(int _userid, std::list<std::string> &_session_list);
    bool GetOfflineIMList(int _userid, std::list<std::string> &_encode_imlist);
    bool GetIMUser(std::set<std::string> &value);

    std::string GetUserId(std::string _phone);
    std::string GetSessionId(std::string _phone);

private:
 //   std::list<redisContext*> free_connection_list_;
	std::map<pthread_t, redisContext*> m_redis_conns;
    std::recursive_mutex list_mutex_;

    int expire_ = REDIS_KEY_EXPIRE_DEFAULT;
	std::string ip;
	int port;
	std::string auth;


};

#endif
