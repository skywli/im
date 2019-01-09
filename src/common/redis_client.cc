#include "redis_client.h"
#include "log_util.h"


#define DB_INDEX_IMUSER 6
#define DB_INDEX_CZYUSER 0
#define DB_INDEX_USERINFO 3
#define DB_INDEX_USER_STATE 8
#define DB_INDEX_IMSAVE 2
#define DB_INDEX_IMPUSH 2
#define DB_INDEX_BROADCAST 14
#define DB_INDEX_SESSION 14
#define DB_INDEX_OFFLINEIM 15
#define DB_INDEX_CZY 0
#define STATE_EXPIRE_TIME    24*3600
#define CZY_ORG_KEY          "czy_us_9959f117-df60-4d1b-a354-776c20ffb8c7"

#define DB_INDEX_BUDDY        1
RedisClient::RedisClient() {

}

RedisClient::~RedisClient() {
   
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

bool RedisClient::GetBroadcastOfflineIMList(int _userid, std::list<std::string>& _out_list)
{
	std::string outdata = "";
	int size = 0;
	bool bValue = false;
	std::string user= "userid:" + std::to_string(_userid);
	if (SelectDB( DB_INDEX_BROADCAST)) {
		if (GetListSize(user.c_str(), outdata)) {
			size = atoi(outdata.c_str());
		}
	}
	for (int i = 0; i < size; i++) {
		if (PopDataFromList( user.c_str(), outdata)) {
			_out_list.push_back(outdata);
		}
	}
	if (!_out_list.empty()) bValue = true;
	
	return bValue;
}

bool RedisClient::GetIMUserList(std::list<std::string> &_keys) {
 
    std::string hash_name = "ImUser:*";
    bool bValue = false;
	if (SelectDB(DB_INDEX_IMUSER)) {
		GetKeysFromHash( hash_name.c_str(), _keys);
		if (!_keys.empty() > 0) {
			bValue = true;
		}
	}
	return bValue;
}



void RedisClient::SetKeysExpire(int _expire) {
    expire_ = _expire == 0 ? REDIS_KEY_EXPIRE_DEFAULT : _expire;
}

bool RedisClient::IsCZYUser(const std::string& user_id) {
    std::string outdata = "";

    bool bValue = false;
	if (SelectDB(DB_INDEX_CZYUSER)) {
		if (SIsMember(CZY_ORG_KEY,user_id.c_str(), outdata)) {
			if (atoi(outdata.c_str()) == 1) {
				bValue = true;
			}
		}
	}
    return bValue;
}

bool RedisClient::HasUserInfo(const std::string& user_id) {
	std::string outdata = "";
    std::string key = std::string("ui_") + user_id;

	bool bValue = false;
	if (SelectDB(DB_INDEX_USERINFO)) {
		if (Check(key.c_str(), outdata)) {
			if (atoi(outdata.c_str()) == 1) {
				bValue = true;
			}
		}
	}
	return bValue;
}

std::string RedisClient::GetUserId(std::string _phone) {
    std::string outdata = "";
    std::string hash_name = "ImUser:";
    hash_name.append(_phone);
	if (SelectDB(DB_INDEX_IMUSER)) {
		GetDataFromHash(hash_name.c_str(), "uid", outdata);
	}
    return outdata;
}

std::string RedisClient::GetSessionId(std::string _phone) {
    std::string outdata = "";
    std::string hash_name = "ImUser:";
    hash_name.append(_phone);

	if (SelectDB( DB_INDEX_IMUSER)) {
		GetDataFromHash( hash_name.c_str(), "sid", outdata);
	}
    return outdata;
}

bool RedisClient::GetUserInfo(std::string _phone, std::list<std::string>& _out_list)
{
	std::string outdata = "";
	std::string hash_name = "ImUser:";
	hash_name.append(_phone);
	bool bValue = false;
	if (SelectDB(DB_INDEX_IMUSER)) {
		GetAllDataFromHash(hash_name.c_str(), _out_list);
		bValue = true;
	}
	return bValue;
}


std::string RedisClient::GetHashValue(const std::string & key, const std::string & field)
{
	std::string outdata = "";
	if (SelectDB(DB_INDEX_BROADCAST)) {
		GetDataFromHash(key.c_str(), field.c_str(), outdata);
	}
	return outdata;
}

bool RedisClient::GetChannelUserList(const std::string & key, int start, int stop, std::list<std::string>& out_list)
{
	bool bValue = false;
	std::string outdata = "";
	if (SelectDB(DB_INDEX_BROADCAST)) {
		GetList(key.c_str(), start, stop, out_list);
	}
	return bValue;
}

bool RedisClient::getUserState(const std::string&  user_id, std::string & _value)
{
	bool bValue = false;
	std::string user_state = std::string("us_") + user_id;
	if (SelectDB(DB_INDEX_USER_STATE)) {
		GetValue(user_state.c_str(), _value);
		bValue = true;
	}
	return bValue;
}

bool  RedisClient::query(const char * command[], int num, size_t * vlen,std::string& value)
{
	bool result = false;
	if (SelectDB(DB_INDEX_BUDDY)) {
		result= HRedisBase::query(command, num,vlen,value);
	}
	return result;
	
}

bool RedisClient::setUserState(const std::string&  user_id, const char* _value)
{
	std::string outdata = "";
	bool bValue = false;
	std::string user_state = std::string("us_") + user_id;
	if (SelectDB(DB_INDEX_USER_STATE)) {
		if (SetExKey(user_state.c_str(), STATE_EXPIRE_TIME, _value, outdata)) {
			if (outdata == "ok" | outdata == "OK") {
				bValue = true;
			}
		}
	}
	return bValue;
}

bool RedisClient::deleteUserState(const std::string& user_id)
{
	std::string outdata = "";
	bool bValue = false;
	std::string user_state = std::string("us_") + user_id;
	if (SelectDB( DB_INDEX_USER_STATE)) {
		if (DelKey(user_state.c_str(), outdata)) {
			if (atoi(outdata.c_str()) == 1) {
				bValue = true;
			}
		}
	}
	return bValue;
}

bool RedisClient::InsertIMtoRedis(const std::string& _imjson) {
  
    std::string outdata = "";
    const char *key = "Imlist";
    bool bValue = false;
	if (SelectDB(DB_INDEX_IMSAVE)) {
		if (SetDataToList(key, _imjson.substr(0, _imjson.length() - 1).c_str(), outdata)) {
			bValue = true;
		}
		else {
			LOGE("InsertIMtoRedis error, key:%s, outdata:%s, content:%s", key, outdata.c_str(), _imjson.c_str());
		}
	}
    return bValue;
}

bool RedisClient::InsertIMPushtoRedis(const std::string& _imjson) {
   
    std::string outdata = "";
    const char *key = "ImPushList";
    bool bValue = false;
	if (SelectDB(DB_INDEX_IMPUSH)) {
		if (SetDataToList(key, _imjson.c_str(), outdata)) {
			bValue = true;
		}
		else {
			LOGE("InsertIMPushtoRedis error, key:%s, outdata:%s, content:%s", key, outdata.c_str(), _imjson.c_str());
		}
	}
    return bValue;
}

bool RedisClient::GetUserSessionList(int _userid, std::list<std::string> &_session_list) {

    std::string outdata = "";
    int size = 0;
    bool bValue = false;

	if (SelectDB(DB_INDEX_SESSION)) {
		if (GetListSize(std::to_string(_userid).c_str(), outdata)) {
			size = atoi(outdata.c_str());
		}
	}
	for (int i = 0; i < size; i++) {
		if (PopDataFromList(std::to_string(_userid).c_str(), outdata)) {
			_session_list.push_back(outdata);
		}
	}
	if (_session_list.size() > 0) bValue = true;
	
    return bValue;
}

bool RedisClient::InsertUserSessionToRedis(int _userid, std::string _session) {
    std::string outdata = "";
    bool bValue = false;
	if (SelectDB(DB_INDEX_SESSION)) {
		//DelListValue(context, std::to_string(_userid).c_str(), _session.c_str(), outdata);
		if (SetDataToList(std::to_string(_userid).c_str(), _session.c_str(), outdata)) {
			SetKeyExpire(std::to_string(_userid).c_str(), expire_, outdata);
			bValue = true;
			//LOGW("InsertUserSessionToRedis success %s %s %s", std::to_string(_userid).c_str(), _session.c_str(), outdata.c_str());
		}
		else {
			//LOGW("InsertUserSessionToRedis failed %s %s %s", std::to_string(_userid).c_str(), _session.c_str(), outdata.c_str());
		}
	}
    return bValue;
}

bool RedisClient::InsertOfflineIMtoRedis(const std::string& user_id, const char *_encode_im) {
   
    std::string outdata = "";
    bool bValue = false;
	if (SelectDB(DB_INDEX_OFFLINEIM)) {
		if (SetDataToList(user_id.c_str(), _encode_im, outdata)) {
			//SetKeyExpire(context, std::to_string(_userid).c_str(), expire_, outdata);
			bValue = true;
		}
	}

    return bValue;
}

bool RedisClient::InsertMsgIdtoRedis(const std::string& user_id, const std::string& id) {

	std::string outdata = "";
	bool bValue = false;
	if (SelectDB(DB_INDEX_OFFLINEIM)) {
		if (SetDataToSet(user_id.c_str(), id.c_str(), outdata)) {
			//SetKeyExpire(context, std::to_string(_userid).c_str(), expire_, outdata);
			bValue = true;
		}
	}
	return bValue;
}

bool RedisClient::DeleteMsgIdtoRedis(const std::string& user_id, const std::string& id) {

	std::string outdata = "";
	bool bValue = false;
	if (SelectDB(DB_INDEX_OFFLINEIM)) {
		if (DelDataToSet(user_id.c_str(), id.c_str(), outdata)) {
			//SetKeyExpire(context, std::to_string(_userid).c_str(), expire_, outdata);
			bValue = true;
		}
	}
	return bValue;
}


bool RedisClient::GetOfflineMsgIds(const std::string& user_id, std::list<std::string> &id_list) {

	std::string outdata = "";
	bool bValue = false;
	if (SelectDB(DB_INDEX_OFFLINEIM)) {
		if (GetDataToSet(user_id.c_str(), id_list)) {
			//SetKeyExpire(context, std::to_string(_userid).c_str(), expire_, outdata);
			bValue = true;
		}
	}
	return bValue;

}

bool RedisClient::InsertBroadcastOfflineIMtoRedis(const std::string& _userid, const std::string& _channel)
{
	std::string outdata = "";
	bool bValue = false;
	if (SelectDB(DB_INDEX_BROADCAST)) {
		if (SetDataToList(_userid.c_str(), _channel.c_str(), outdata)) {
			//SetKeyExpire(context, std::to_string(_userid).c_str(), expire_, outdata);
			bValue = true;
		}
	}
	
	return bValue;
}

std::string RedisClient::GetBroadcastMsg(const std::string& _key)
{
	
	std::string outdata = "";
	if (SelectDB(DB_INDEX_BROADCAST)) {
		GetValue(_key.c_str(), outdata);
	}
	return outdata;
}

bool RedisClient::GetOfflineIMList(int _userid, std::list<std::string> &_encode_imlist) {
    std::string outdata = "";
    int size = 0;
    bool bValue = false;

	if (SelectDB(DB_INDEX_OFFLINEIM)) {
		if (GetListSize(std::to_string(_userid).c_str(), outdata)) {
			size = atoi(outdata.c_str());
		}
	}
	for (int i = 0; i < size; i++) {
		if (PopDataFromList(std::to_string(_userid).c_str(), outdata)) {
			_encode_imlist.push_back(outdata);
		}
	}
	if (!_encode_imlist.empty()) bValue = true;
	
    return bValue;
}
