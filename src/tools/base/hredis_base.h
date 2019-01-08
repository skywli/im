#ifndef _HREDIS_BASE_H
#define _HREDIS_BASE_H

#include <hiredis/hiredis.h>
#include <string>
#include <list>
#include <strings.h>
#include <pthread.h>
#include <mutex>
#include <map>

class HRedisBase {
public:
    HRedisBase();
	~HRedisBase();
	redisContext * ConnectRedis(std::string & ip, int port, std::string & auth);

	redisContext * getConnect();
	void deleteConnect();
	bool Connect(std::string _ip, int _port, redisContext *&_context);
    bool Auth(redisContext *_context,std::string _password);
    bool SelectDB(int index);
    bool ExecuteCmd(std::string &_outdata, const char *_cmd, ...);
	bool query(const char * command[], int num, size_t * vlen, std::string & value);
    bool ExecuteCmd(std::string &_outdata, const char *_cmd, va_list ap);
    bool ExecuteCmd( std::list<std::string> &_outlist, const char *_cmd, ...);
    bool ExecuteCmd( std::list<std::string> &_outlist, const char *_cmd, va_list ap);

public:
    bool Check( const char *_key, std::string &_outdata);
	bool SIsMember(const char * _key, const char * value, std::string & _outdata);
    bool SetKeyExpire( const char *_key, int _expire, std::string &_outdata);
	bool SetValue( const char * _key, const char * _value, std::string & _outdata);
	bool DelKey( const char * _key, std::string & _outdata);
	bool GetValue( const char * _key, std::string & _outdata);
	bool SetExKey( const char * _key, int _expire, const char * _value, std::string & _outdata);
    bool GetListSize( const char *_key, std::string &_outdata);
	bool GetList( const char *_key, int start,int stop,std::list<std::string> &_outlist);
    bool DelListValue( const char *_key, const char *_value, std::string &_outdata);
    bool SetDataToList( const char *_list_name, const char *_value, std::string &_outdata);
	bool SetDataToSet(const char * _set_name, const char * _value, std::string & _outdata);
	bool DelDataToSet(const char * _set_name, const char * _value, std::string & _outdata);
	bool GetDataToSet(const char * _set_name, std::list<std::string>& _outlist);
    bool PopDataFromList( const char *_key, std::string &_outdata);
    bool GetDataFromHash(const char *_hash_name, const char *_key, std::string &_outdata);
	bool GetAllDataFromHash( const char * _hash_name, std::list<std::string>& _outlist);
    bool GetKeysFromHash( const char *_pattern, std::list<std::string> &_outlist);
	bool GetKeysFromHashByScan( const char * _pattern, std::list<std::string>& _outlist);

protected:
	std::string ip;
	int port;
	std::string auth;

private:
	std::map<pthread_t, redisContext*> m_redis_conns;
	std::recursive_mutex list_mutex_;
};

#endif
