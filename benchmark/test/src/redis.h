#ifndef _REDIS_CACHE_H
#define _REDIS_CACHE_H

#include <hiredis/hiredis.h>
#include<string>
#include <list>
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
	bool getReply(std::string & outstr);
private:
	int selectDb(int index);
	redisReply * executeCmd(const char * command[], int num, size_t * vlen);
	redisReply * getReply();
	
private:
	redisContext*               context_;
	std::string                 ip_;
	short                       port_;
	std::string                 auth_;
	int                         index_;
};
#endif
