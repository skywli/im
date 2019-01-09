#ifndef _HREDIS_BASE_H
#define _HREDIS_BASE_H

#include <hiredis/hiredis.h>
#include <string>
#include <list>
#include <strings.h>
#include <vector>

class HRedisBase {
public:
    HRedisBase();

    bool Connect(std::string _ip, int _port, redisContext *&_context);
    bool Auth(redisContext *_context, std::string _password);
    bool SelectDB(redisContext *_context, int index);
    bool ExecuteCmd(redisContext *_context, std::string &_outdata, const char *_cmd, ...);
    bool ExecuteCmd(redisContext *_context, std::string &_outdata, const char *_cmd, va_list ap);
    bool ExecuteCmd(redisContext *_context, std::list<std::string> &_outlist, const char *_cmd, ...);
    bool ExecuteCmd(redisContext *_context, std::list<std::string> &_outlist, const char *_cmd, va_list ap);

public:
    bool Check(redisContext *_context, const char *_key, std::string &_outdata);
    bool SetKeyExpire(redisContext *_context, const char *_key, int _expire, std::string &_outdata);
    bool GetListSize(redisContext *_context, const char *_key, std::string &_outdata);
    bool DelListValue(redisContext *_context, const char *_key, const char *_value, std::string &_outdata);
    bool SetDataToList(redisContext *_context, const char *_list_name, const char *_value, std::string &_outdata);
    bool PopDataFromList(redisContext *_context, const char *_key, std::string &_outdata);
    bool GetDataFromHash(redisContext *_context, const char *_hash_name, const char *_key, std::string &_outdata);
    bool GetKeysFromHash(redisContext *_context, const char *_pattern, std::list<std::string> &_outlist);
	bool GetMembers(redisContext * _context, const char * _pattern, std::list<std::string>& _outlist);
	
	
};

#endif
