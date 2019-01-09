#ifndef _REDIS_CACHE_H
#define _REDIS_CACHE_H

#include <hiredis/hiredis.h>
#include<string>
#include <list>
#include <mutex>
#include <map>
#include <pthread.h>
class Redis {
public:
	Redis();
	~Redis();
	void init();
	int  connect(const std::string& ip, short port,const std::string& auth,int index);
	bool query(const char * command[], int num, size_t * vlen, std::string & outstr);
	bool query(const char * command[], int num, size_t * vlen, std::list<std::string> & outlist);
	bool query(const char * command[], int num, size_t * vlen);

	//after call appendCommandArgv  then call getReply
	bool  appendCommandArgv(const char * command[], int num, size_t * vlen);
	int getReply(int num);
private:
	class                      RedisInstance;
	
	RedisInstance * getRedisInstance();
	
private:
	
	std::string                 ip_;
	short                       port_;
	std::string                 auth_;
	int                         index_;

	std::map<pthread_t, RedisInstance*>  m_redis_conns;
	std::recursive_mutex                 m_conn_mutex_;
};
#endif
