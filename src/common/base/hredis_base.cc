#include "hredis_base.h"
#include <memory>
#include <thread>
#include<log_util.h>
HRedisBase::HRedisBase() {

}
HRedisBase::~HRedisBase()
{
	std::map<pthread_t, redisContext*>::iterator it = m_redis_conns.begin();
	while (it != m_redis_conns.end()) {
		redisFree(it->second);
		m_redis_conns.erase(it++);
	}
}
redisContext * HRedisBase::ConnectRedis(std::string & ip, int port, std::string& auth)
{
	redisContext *context = NULL;
	if (Connect(ip, port, context)) {
		if (Auth(context, auth)) {
			return context;
		}
		LOGE("redis auth:%s fail",auth.c_str());
	}
	else {
		LOGE("connect redis[ip:%s port:%d ]fail", ip.c_str(), port);
	}
	
	return NULL;
}

redisContext * HRedisBase::getConnect()
{
	pthread_t  id = pthread_self();
	std::lock_guard<std::recursive_mutex> lock_1(list_mutex_);
	std::map<pthread_t, redisContext*>::iterator it = m_redis_conns.find(id);
	if (it != m_redis_conns.end()) {
		return it->second;
	}
	else {
		redisContext* client = ConnectRedis(ip, port, auth);
		if (client) {
			m_redis_conns.insert(std::pair<pthread_t, redisContext*>(id, client));
			return client;
		}
	}
	return NULL;
}

void HRedisBase::deleteConnect()
{
	pthread_t  id = pthread_self();
	std::lock_guard<std::recursive_mutex> lock_1(list_mutex_);
	std::map<pthread_t, redisContext*>::iterator it = m_redis_conns.find(id);
	if (it != m_redis_conns.end()) {
		redisFree(it->second);
		m_redis_conns.erase(it++);
	}
	
	return ;
}


bool HRedisBase::Connect(std::string _ip, int _port, redisContext *&_context) {
    struct timeval timeout = {1, 500000}; // 1.5 seconds

    _context = redisConnectWithTimeout(_ip.c_str(), _port, timeout);
    if (_context == NULL) {
        return false;
    } else if (_context->err) {
        redisFree(_context);
        return false;
    }
    return true;
}

bool HRedisBase::Auth(redisContext * context ,std::string _password) {
    std::string outdata;

    redisReply *reply = (redisReply*)redisCommand(context, "AUTH %s", _password.c_str());
    if (!reply) {
		LOGE("execute cmd(auth) fail");
        return false;

    }
    if(reply->type==REDIS_REPLY_ERROR){
        LOGE("%s",reply->str);
        return true;
    }
            
    return true;
}

bool HRedisBase::SelectDB( int index) {
    std::string outdata;

    if (ExecuteCmd(outdata, "SELECT %d", index)) {
        if (strcasecmp(outdata.c_str(), "OK") == 0) {
            return true;
        }
    }
    return false;
}

bool HRedisBase::Check( const char *_key, std::string &_outdata) {
    return ExecuteCmd(_outdata, "EXISTS %s", _key);
}

bool HRedisBase::SIsMember(const char *_key, const char* value,std::string &_outdata) {
	return ExecuteCmd(_outdata, "SISMEMBER %s %s", _key,value);
}

bool HRedisBase::SetKeyExpire( const char *_key, int _expire, std::string &_outdata) {
    return ExecuteCmd(_outdata, "EXPIRE %s %d", _key, _expire);
}

bool HRedisBase::SetValue( const char * _key, const char * _value, std::string & _outdata)
{
	return ExecuteCmd( _outdata, "SET %s  %s", _key,  _value);
}

bool HRedisBase::DelKey( const char * _key,  std::string & _outdata)
{
	return ExecuteCmd( _outdata, "DEL %s", _key);
}

bool HRedisBase::GetValue( const char *_key, std::string &_outdata) {
	return ExecuteCmd( _outdata, "GET %s ", _key);
}

bool HRedisBase::SetExKey( const char *_key, int _expire,const char* _value, std::string &_outdata) {
	return ExecuteCmd(_outdata, "SETEX %s %d %s", _key, _expire,_value);
}

bool HRedisBase::GetListSize( const char *_key, std::string &_outdata) {
    return ExecuteCmd(_outdata, "LLEN %s", _key);
}

bool HRedisBase::GetList( const char *_key, int start,int stop,std::list<std::string> &_outlist) {
    return ExecuteCmd(_outlist, "LRANGE  %s %d  %d", _key,start,stop);
}


bool HRedisBase::DelListValue( const char *_key, const char *_value, std::string &_outdata) {
    return ExecuteCmd(_outdata, "LREM %d 0 %d", _key, _value);
}

bool HRedisBase::SetDataToList( const char *_list_name, const char *_value, std::string &_outdata) {
    return ExecuteCmd(_outdata, "RPUSH %s %s", _list_name, _value);
}

bool HRedisBase::SetDataToSet(const char *_set_name, const char *_value, std::string &_outdata) {
	return ExecuteCmd(_outdata, "SADD %s %s", _set_name, _value);
}

bool HRedisBase::DelDataToSet(const char *_set_name, const char *_value, std::string &_outdata) {
	return ExecuteCmd(_outdata, "SREM %s %s", _set_name, _value);
}

bool HRedisBase::GetDataToSet(const char *_set_name, std::list<std::string> &_outlist) {
	return ExecuteCmd(_outlist, "SMEMBERS %s ", _set_name);
}

bool HRedisBase::PopDataFromList( const char *_key, std::string &_outdata) {
    return ExecuteCmd(_outdata, "LPOP %s", _key);
}

bool HRedisBase::GetDataFromHash( const char *_hash_name, const char *_key, std::string &_outdata) {
    return ExecuteCmd(_outdata, "HGET %s %s", _hash_name, _key);
}

bool HRedisBase::GetAllDataFromHash( const char *_hash_name, std::list<std::string> &_outlist) {
	return ExecuteCmd(_outlist, "HGETALL %s ", _hash_name);
}

bool HRedisBase::GetKeysFromHash( const char *_pattern, std::list<std::string> &_outlist) {
    return ExecuteCmd(_outlist, "KEYS %s", _pattern);
}

bool HRedisBase::GetKeysFromHashByScan( const char *_pattern, std::list<std::string> &_outlist) {
	return ExecuteCmd(_outlist, "SSCAN %s", _pattern);
}

bool HRedisBase::ExecuteCmd( std::string &_outdata, const char *_cmd, ...) {
    va_list ap;

    /*
    va_start(ap, _cmd);
    char buf[2048] = {0};
    vsprintf(buf, _cmd, ap);
    LOGW("cmd:%s", buf);
    va_end(ap);
    */

    va_start(ap, _cmd);
    bool ret = ExecuteCmd(_outdata, _cmd, ap);
    va_end(ap);
    return ret;
}
bool HRedisBase::query(const char * command[], int num, size_t * vlen,std::string& value)
{
	bool result = false;

	redisContext* context = getConnect();
	if (!context) {
		return false;
	}

	int i;
	redisReply *reply;
	for (i = 0; i < 2; i++) {
		reply = (redisReply*)redisCommandArgv(context, num, command, vlen);
		if (reply == NULL) {
			deleteConnect();
			LOGE("try again get connection");

			redisContext* context = getConnect();
			if (!context) {
				return false;
			}
			continue;
		}
		break;
	}
	if (reply == NULL) {
		return false;
	}

	std::shared_ptr<redisReply> autoFree(reply, freeReplyObject);

	if (reply->type == REDIS_REPLY_STRING) {
		value.append(reply->str, reply->len);
		result = true;
	}
	else if (reply->type == REDIS_REPLY_ERROR) {
		LOGE("query redis fail");
		result = false;
	}
	return result;
}
//not user para _context
bool HRedisBase::ExecuteCmd( std::string &_outdata, const char *_cmd, va_list ap) {
    bool result = false;

	redisContext* context = getConnect();
	if (!context) {
		return false;
	}
	redisReply *reply;
	int i;
	for (i = 0; i < 2; i++) {
		reply = (redisReply*)redisvCommand(context, _cmd, ap);
		if (reply == NULL) {
			LOGE("execute cmd(%s) fail", _cmd);
			deleteConnect();
			return result;
			LOGE("try again get connection");

			redisContext* context = getConnect();
			if (!context) {
				return false;
			}
			continue;
		}
		break;
	}
	if (reply == NULL) {
		return false;
	}
    std::shared_ptr<redisReply> autoFree(reply, freeReplyObject);
    //LOGW("type:%d, str:%s, integer:%d", reply->type, reply->str, reply->integer);

    switch (reply->type) {
    case REDIS_REPLY_INTEGER:
        _outdata = std::to_string(reply->integer);
        result = true;
        break;
    case REDIS_REPLY_STRING:
        _outdata = reply->str;
        result = true;
        break;
    case REDIS_REPLY_STATUS:
        _outdata = reply->str;
        result = true;
        break;
    case REDIS_REPLY_NIL:
        _outdata = "";
        result = true;
        break;
    case REDIS_REPLY_ERROR:
        _outdata = reply->str;
        result = false;
        break;
    default:
        _outdata = "";
        result = false;
        break;
    }
    return result;
}

bool HRedisBase::ExecuteCmd( std::list<std::string> &_outlist, const char *_cmd, ...) {
    va_list ap;
    va_start(ap, _cmd);
    bool ret = ExecuteCmd(_outlist, _cmd, ap);
    va_end(ap);
    return ret;
}

bool HRedisBase::ExecuteCmd( std::list<std::string> &_outlist, const char *_cmd, va_list ap) {
    bool result = false;

	redisContext* context = getConnect();
	if (!context) {
		return false;
	}

    redisReply *reply = (redisReply*)redisvCommand(context, _cmd, ap);
    if (reply == NULL) {
		LOGE("execute cmd(%s) fail", _cmd);
		deleteConnect();
		return result;
    }

    std::shared_ptr<redisReply> autoFree(reply, freeReplyObject);
    //LOGW("cmd:%s, type:%d, str:%s, integer:%d", _cmd, reply->type, reply->str, reply->integer);

    switch (reply->type) {
    case REDIS_REPLY_ARRAY:
        for (int i = 0; i < reply->elements; i++) {
            redisReply *child = reply->element[i];
            if (child->type == REDIS_REPLY_STRING) {
                _outlist.push_back(child->str);
            }
        }
        result = true;
        break;
    default:
        result = false;
        break;
    }
    return result;
}
