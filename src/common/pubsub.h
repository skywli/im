#pragma once
#include <string>
#include <event.h>
#include <hiredis/adapters/libevent.h>
#include <redis.h>
namespace State {
	typedef void(*subscribeCb)(const char* data);
class StateSub {
public:
	StateSub(const std::string& ip, short port, const std::string auth = "") :ip_(ip), port_(port), auth_(auth), base_(NULL) {}
	int init();
	int  subscribe(const std::string& channel);
private:
	int initialize();
	static void connectCallback(const redisAsyncContext * c, int status);
	static void disconnectCallback(const redisAsyncContext * c, int status);
	static void subCallback(redisAsyncContext * c, void * r, void * priv);
private:
	std::string ip_;
	short port_;
	std::string auth_;
	struct event_base *base_;
    redisAsyncContext *c;
};

class StatePub {
public:
	StatePub(const std::string& ip, short port, const std::string auth = "") :ip_(ip), port_(port), auth_(auth) {}
	int init();
	int  publish(const std::string& channel,const char* data,int len);
private:
	std::string ip_;
	short port_;
	std::string auth_;
	Redis   client_;

};
}
