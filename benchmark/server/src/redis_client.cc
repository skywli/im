#include "redis_client.h"
#include "log_util.h"

#include <thread>
#define DB_INDEX_IMUSER 6
#define DB_INDEX_IMSAVE 2
#define DB_INDEX_IMPUSH 2
#define DB_INDEX_SESSION 14
#define DB_INDEX_OFFLINEIM 15

RedisClient::RedisClient() {

}

RedisClient::~RedisClient() {
    std::map<pthread_t,redisContext*>::iterator it=m_redis_conns.begin();
    while(it!=m_redis_conns.end()){
        redisFree(it->second);
        m_redis_conns.erase(it++);
    }
}

void RedisClient::UnInit_Pool() {
    LOGT("Redis数据库unInit");
    std::map<pthread_t,redisContext*>::iterator it=m_redis_conns.begin();
    while(it!=m_redis_conns.end()){
        redisFree(it->second);
        m_redis_conns.erase(it++);
    }
}

void RedisClient::Init_Pool(std::string _ip, int _port, std::string _auth, int _num) {
    LOGT("Redis数据库初始化 %s, %d, %d", _ip.c_str(), _port, _num);
	ip = _ip;
	port = _port;
	auth = _auth;
   /* for (int i = 0; i < _num; i++) {
        redisContext *context = NULL;
        if (Connect(_ip, _port, context)) {
            if (Auth(context, _auth)) {
                free_connection_list_.push_back(context);
            }
        }
    }
    if (free_connection_list_.size() == 0) {
        LOGE("Redis数据库连接失败，进程结束");
        exit(0);
    }*/
}

redisContext * RedisClient::ConnectRedis(std::string & ip, int port,std::string& auth)
{
	redisContext *context = NULL;
	if (Connect(ip, port, context)) {
		if (Auth(context, auth)) {
			return context;
		}
	}
	return NULL;
}

redisContext * RedisClient::getConnect()
{
	pthread_t  id = pthread_self();
	std::lock_guard<std::recursive_mutex> lock_1(list_mutex_);
	std::map<pthread_t, redisContext*>::iterator it = m_redis_conns.find(id);
	if (it != m_redis_conns.end()) {
		return it->second;
	}
	else {
		redisContext* client = ConnectRedis(ip,port,auth);
		if (client) {
			m_redis_conns.insert(std::pair<pthread_t, redisContext*>(id, client));
		    return client;
		}
	}
	return NULL;
}



bool RedisClient::GetIMUser(std::vector<std::string> &value) {
    redisContext *context = NULL;
    std::string hash_name = "ImUser:*";
    bool bValue = false;
    std::list<std::string> _keys;
	context = getConnect();
	if (context) {
		if (SelectDB(context, 0)) {
			GetMembers(context, "userIds", _keys);
			if (_keys.size() > 0) {
				bValue = true;
                copy(_keys.begin(),_keys.end(),back_inserter(value));
			}
		}
	}
    return bValue;
}

void RedisClient::SetKeysExpire(int _expire) {
    expire_ = _expire == 0 ? REDIS_KEY_EXPIRE_DEFAULT : _expire;
}

bool RedisClient::IsHuxinUser(std::string _phone) {
    redisContext *context = NULL;
    std::string outdata = "";
    std::string hash_name = "ImUser:";
    hash_name.append(_phone);

    bool bValue = false;
	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_IMUSER)) {
			if (Check(context, hash_name.c_str(), outdata)) {
				if (atoi(outdata.c_str()) == 1) {
					bValue = true;
				}
			}
		}
	}

    return bValue;
}

std::string RedisClient::GetUserId(std::string _phone) {
    redisContext *context = NULL;
    std::string outdata = "";
    std::string hash_name = "ImUser:";
    hash_name.append(_phone);

	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_IMUSER)) {
			GetDataFromHash(context, hash_name.c_str(), "uid", outdata);
		}
	}
    return outdata;
}

std::string RedisClient::GetSessionId(std::string _phone) {
    redisContext *context = NULL;
    std::string outdata = "";
    std::string hash_name = "ImUser:";
    hash_name.append(_phone);

	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_IMUSER)) {
			GetDataFromHash(context, hash_name.c_str(), "sid", outdata);
		}
	}
    return outdata;
}

bool RedisClient::InsertIMtoRedis(std::string _imjson) {
    redisContext *context = NULL;
    std::string outdata = "";
    const char *key = "Imlist";
    bool bValue = false;

	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_IMSAVE)) {
			if (SetDataToList(context, key, _imjson.substr(0, _imjson.length() - 1).c_str(), outdata)) {
				bValue = true;
			}
			else {
				LOGE("InsertIMtoRedis error, key:%s, outdata:%s, content:%s", key, outdata.c_str(), _imjson.c_str());
			}
		}
	}
    return bValue;
}

bool RedisClient::InsertIMPushtoRedis(std::string _imjson) {
    redisContext *context = NULL;
    std::string outdata = "";
    const char *key = "ImPushList";
    bool bValue = false;

	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_IMPUSH)) {
			if (SetDataToList(context, key, _imjson.substr(0, _imjson.length() - 1).c_str(), outdata)) {
				bValue = true;
			}
			else {
				LOGE("InsertIMPushtoRedis error, key:%s, outdata:%s, content:%s", key, outdata.c_str(), _imjson.c_str());
			}
		}
	}
    return bValue;
}

bool RedisClient::GetUserSessionList(int _userid, std::list<std::string> &_session_list) {
    redisContext *context = NULL;
    std::string outdata = "";
    int size = 0;
    bool bValue = false;

	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_SESSION)) {
			if (GetListSize(context, std::to_string(_userid).c_str(), outdata)) {
				size = atoi(outdata.c_str());
			}
		}
		for (int i = 0; i < size; i++) {
			if (PopDataFromList(context, std::to_string(_userid).c_str(), outdata)) {
				_session_list.push_back(outdata);
			}
		}
		if (_session_list.size() > 0) bValue = true;

	}
    return bValue;
}

bool RedisClient::InsertUserSessionToRedis(int _userid, std::string _session) {
    redisContext *context = NULL;
    std::string outdata = "";
    bool bValue = false;

	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_SESSION)) {
			//DelListValue(context, std::to_string(_userid).c_str(), _session.c_str(), outdata);
			if (SetDataToList(context, std::to_string(_userid).c_str(), _session.c_str(), outdata)) {
				SetKeyExpire(context, std::to_string(_userid).c_str(), expire_, outdata);
				bValue = true;
				//LOGW("InsertUserSessionToRedis success %s %s %s", std::to_string(_userid).c_str(), _session.c_str(), outdata.c_str());
			}
			else {
				//LOGW("InsertUserSessionToRedis failed %s %s %s", std::to_string(_userid).c_str(), _session.c_str(), outdata.c_str());
			}
		}
	}
    return bValue;
}

bool RedisClient::InsertOfflineIMtoRedis(int _userid, const char *_encode_im) {
    redisContext *context = NULL;
    std::string outdata = "";
    bool bValue = false;
	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_OFFLINEIM)) {
			if (SetDataToList(context, std::to_string(_userid).c_str(), _encode_im, outdata)) {
				//SetKeyExpire(context, std::to_string(_userid).c_str(), expire_, outdata);
				bValue = true;
			}
		}
	}
    return bValue;
}

bool RedisClient::GetOfflineIMList(int _userid, std::list<std::string> &_encode_imlist) {
    redisContext *context = NULL;
    std::string outdata = "";
    int size = 0;
    bool bValue = false;

	context = getConnect();
	if (context) {
		if (SelectDB(context, DB_INDEX_OFFLINEIM)) {
			if (GetListSize(context, std::to_string(_userid).c_str(), outdata)) {
				size = atoi(outdata.c_str());
			}
		}
		for (int i = 0; i < size; i++) {
			if (PopDataFromList(context, std::to_string(_userid).c_str(), outdata)) {
				_encode_imlist.push_back(outdata);
			}
		}
		if (_encode_imlist.size() > 0) bValue = true;

	}
    return bValue;
}
