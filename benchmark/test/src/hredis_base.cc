#include "hredis_base.h"
#include <memory>

HRedisBase::HRedisBase() {

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

bool HRedisBase::Auth(redisContext *_context, std::string _password) {
    std::string outdata;

    if (ExecuteCmd(_context, outdata, "AUTH %s", _password.c_str())) {
        if (strcasecmp(outdata.c_str(), "OK") == 0) {
            return true;
        }
    }
    return true;
}

bool HRedisBase::SelectDB(redisContext *_context, int index) {
    std::string outdata;

    if (ExecuteCmd(_context, outdata, "SELECT %d", index)) {
        if (strcasecmp(outdata.c_str(), "OK") == 0) {
            return true;
        }
    }
    return false;
}

bool HRedisBase::Check(redisContext *_context, const char *_key, std::string &_outdata) {
    return ExecuteCmd(_context, _outdata, "EXISTS %s", _key);
}

bool HRedisBase::SetKeyExpire(redisContext *_context, const char *_key, int _expire, std::string &_outdata) {
    return ExecuteCmd(_context, _outdata, "EXPIRE %s %d", _key, _expire);
}

bool HRedisBase::GetListSize(redisContext *_context, const char *_key, std::string &_outdata) {
    return ExecuteCmd(_context, _outdata, "LLEN %s", _key);
}

bool HRedisBase::DelListValue(redisContext *_context, const char *_key, const char *_value, std::string &_outdata) {
    return ExecuteCmd(_context, _outdata, "LREM %d 0 %d", _key, _value);
}

bool HRedisBase::SetDataToList(redisContext *_context, const char *_list_name, const char *_value, std::string &_outdata) {
    return ExecuteCmd(_context, _outdata, "RPUSH %s %s", _list_name, _value);
}

bool HRedisBase::PopDataFromList(redisContext *_context, const char *_key, std::string &_outdata) {
    return ExecuteCmd(_context, _outdata, "LPOP %s", _key);
}

bool HRedisBase::GetDataFromHash(redisContext *_context, const char *_hash_name, const char *_key, std::string &_outdata) {
    return ExecuteCmd(_context, _outdata, "HGET %s %s", _hash_name, _key);
}

bool HRedisBase::GetKeysFromHash(redisContext *_context, const char *_pattern, std::list<std::string> &_outlist) {
    return ExecuteCmd(_context, _outlist, "KEYS %s", _pattern);
}

bool HRedisBase::GetMembers(redisContext *_context, const char *_pattern, std::list<std::string> &_outlist) {
	return ExecuteCmd(_context, _outlist, "SMEMBERS %s", _pattern);
}

bool HRedisBase::ExecuteCmd(redisContext *_context, std::string &_outdata, const char *_cmd, ...) {
    va_list ap;

    /*
    va_start(ap, _cmd);
    char buf[2048] = {0};
    vsprintf(buf, _cmd, ap);
    LOGW("cmd:%s", buf);
    va_end(ap);
    */

    va_start(ap, _cmd);
    bool ret = ExecuteCmd(_context, _outdata, _cmd, ap);
    va_end(ap);
    return ret;
}

bool HRedisBase::ExecuteCmd(redisContext *_context, std::string &_outdata, const char *_cmd, va_list ap) {
    bool result = false;

    if (_context == NULL) {
        return result;
    }

    redisReply *reply = (redisReply*)redisvCommand(_context, _cmd, ap);
    if (reply == NULL) {
        return result;
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

bool HRedisBase::ExecuteCmd(redisContext *_context, std::list<std::string> &_outlist, const char *_cmd, ...) {
    va_list ap;
    va_start(ap, _cmd);
    bool ret = ExecuteCmd(_context, _outlist, _cmd, ap);
    va_end(ap);
    return ret;
}

bool HRedisBase::ExecuteCmd(redisContext *_context, std::list<std::string> &_outlist, const char *_cmd, va_list ap) {
    bool result = false;

    if (_context == NULL) {
        return result;
    }

    redisReply *reply = (redisReply*)redisvCommand(_context, _cmd, ap);
    if (reply == NULL) {
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
