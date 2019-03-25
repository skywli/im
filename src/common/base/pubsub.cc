#include <cstring>
#include <log_util.h>
#include "pubsub.h"
namespace State {
	int StateSub::init()
	{
		base_ = event_base_new();

		if (initialize() == -1) {
			return -1;
		}
		event_base_dispatch(base_);
		return 0;

	}

	int StateSub::subscribe(const std::string & channel)
	{
		char data[100] = { 0 };
		sprintf(data, "SUBSCRIBE %s", channel.c_str());
		redisAsyncCommand(c, subCallback, NULL, data);
		return 0;
	}

	int StateSub::initialize()
	{
		c = redisAsyncConnect(ip_.c_str(), port_);
		if (c->err) {
			LOGE("Error: %s", c->errstr);
			return 1;
		}
		c->data = this;
		redisLibeventAttach(c, base_);
		redisAsyncSetConnectCallback(c, connectCallback);
		redisAsyncSetDisconnectCallback(c, disconnectCallback);

		if (auth_ != "") {
			redisAsyncCommand(c, NULL, (char*) "auth", auth_.c_str());
		}
		return 0;
	}

	void StateSub::connectCallback(const redisAsyncContext *c, int status) {
		if (status != REDIS_OK) {
			LOGE("Error: %s", c->errstr);
			return;
		}
		LOGI("Connected...");
	}

	void StateSub::disconnectCallback(const redisAsyncContext *c, int status) {
		if (status != REDIS_OK) {
			LOGE("Error: %s\n", c->errstr);
			return;
		}
		LOGE("Disconnected... reconnect");
		StateSub* sub = reinterpret_cast<StateSub*>(c->data);
		if (sub) {
			sub->initialize();
		}
	}

	void  StateSub::subCallback(redisAsyncContext *c, void *r, void *priv) {
		redisReply *reply = (redisReply*)r;
		if (reply == NULL) return;
		if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
			if (strcmp(reply->element[0]->str, "subscribe") != 0) {
				printf("Received channel %s: %s\n",
					reply->element[1]->str,
					reply->element[2]->str);
			}
		}
		if (reply->type == REDIS_REPLY_ERROR) {
			LOGE("subscribe error");
		}
	}

	int StatePub::init()
	{
		if (client_.connect(ip_, port_, auth_, 0) != 0) {
			LOGE("redis: connect redis ip:%s,port:%d fail", ip_.c_str(), port_);
			return -1;
		}
		return 0;
	}

	int StatePub::publish(const std::string& channel, const char* data, int len)
	{
		const char *command[3];
		size_t vlen[3];

		command[0] = "publish";
		command[1] = channel.c_str();
		command[2] = data;

		vlen[0] = 7;
		vlen[1] = channel.length();
		vlen[2] = len;

		std::string value;
		bool res = client_.query(command, sizeof(command) / sizeof(command[0]), vlen, value);
		if (!res) {
			return -1;
		}
		return 0;
	}
}
