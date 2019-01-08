#include "redis.h"
#include <log_util.h>
Redis::Redis()
{
	context_ = NULL;
	init();
}
Redis::~Redis()
{
	if (context_) {
		redisFree(context_);
	}
}
void Redis::init()
{
#ifdef WIN32  
	WSADATA t_wsa;
	WORD wVers = MAKEWORD(2, 2); // Set the version number to 2.2
	int iError = WSAStartup(wVers, &t_wsa);

	if (iError != NO_ERROR || LOBYTE(t_wsa.wVersion) != 2 || HIBYTE(t_wsa.wVersion) != 2) {
		LogMsg(LOG_ERROR, "Winsock2 init error");
		return ;
	}
#endif
}

int Redis::connect(const std::string & ip, short port, const std::string & auth,int index)
{
	ip_ = ip;
	port_ = port;
	auth_ = auth;
	index_ = index;
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	redisContext* context = redisConnectWithTimeout(ip.c_str(), port, tv);
	if (context && !context->err)
	{
		context_ = context;

		if (auth.length() > 0) {
			redisReply *reply = (redisReply*)redisCommand(context, "AUTH %s", auth.c_str());
			if (!reply) {
				LOGE("redis: execute cmd(auth) fail");
				return -1;

			}
			if (reply->type == REDIS_REPLY_ERROR) {
				LOGE("redis: %s", reply->str);
				return -1;
			}
			if (!selectDb(index)) {
				LOGE("redis: select db index: %d fail",index );
				return -1;
			}
		}
		return 0;
	}
	
	else {
		if (context_ == NULL) {
			LOGE("redis: connect redis error ");
			return -1;
		}
		else if (context_->err) {
			LOGE("redis: connect redis error %s", context->errstr);
			redisFree(context_);
			context_ = NULL;
			return -1;
		}
	}
	return 0;
}


int Redis::selectDb(int index)
{
	std::string id = std::to_string(index);
	const char *command[2];
	size_t vlen[2];
	command[0] = "select";
	vlen[0] = 6;
	command[1] = id.c_str();
	vlen[1] = id.length();
	return query(command, sizeof(command) / sizeof(command[0]), vlen);
}

bool  Redis::query(const char * command[], int num, size_t * vlen,std::string& outstr)
{
	bool result = false;
	redisReply* reply = executeCmd(command, num, vlen);
	if (reply) {
		switch (reply->type) {
		case REDIS_REPLY_INTEGER:
			outstr = std::to_string(reply->integer);
			result = true;
			break;
		case REDIS_REPLY_STRING:
			outstr = std::string(reply->str,reply->len);
			result = true;
			break;
		case REDIS_REPLY_STATUS:
			outstr = reply->str;
			result = true;
			break;
		case REDIS_REPLY_NIL:
			outstr = "";
			result = false;
			break;
		case REDIS_REPLY_ERROR:
			outstr = reply->str;
			result = false;
			break;
		default:
			outstr = "";
			result = false;
			break;
		}
	}
	if (reply) {
		freeReplyObject(reply);
	}
	return result;
	
}

bool Redis::query(const char * command[], int num, size_t * vlen, std::list<std::string>& outlist)
{
	bool result = false;
	redisReply* reply = executeCmd(command, num, vlen);
	if (reply) {
		switch (reply->type) {
		case REDIS_REPLY_ARRAY:
			for (int i = 0; i < reply->elements; i++) {
				redisReply *child = reply->element[i];
				if (child->type == REDIS_REPLY_STRING) {
					outlist.push_back(child->str);
				}
			}
			result = true;
			break;
		default:
			result = false;
			break;
		}
		
	}
	if (reply) {
		freeReplyObject(reply);
	}
	return result;
}

bool Redis::query(const char * command[], int num, size_t * vlen)
{
	bool result = false;
	redisReply* reply = executeCmd(command, num, vlen);
	if (reply) {
		switch (reply->type) {
		case REDIS_REPLY_NIL:
			result = false;
			break;
		case REDIS_REPLY_ERROR:
			result = false;
			break;
		default:
			result = true;
			break;
		}
	}
	if (reply) {
		freeReplyObject(reply);
	}
	return result;
}

bool Redis::appendCommandArgv(const char * command[], int num, size_t * vlen)
{
	if (redisAppendCommandArgv(context_, num, command, vlen) == 0) {
		return true;
	}
	return false;
}

bool Redis::getReply(std::string & outstr)
{
	bool result = false;
	redisReply *reply = getReply();
	if (reply) {
		switch (reply->type) {
		case REDIS_REPLY_INTEGER:
			outstr = std::to_string(reply->integer);
			result = true;
			break;
		case REDIS_REPLY_STRING:
			outstr = std::string(reply->str, reply->len);
			result = true;
			break;
		case REDIS_REPLY_STATUS:
			outstr = reply->str;
			result = true;
			break;
		case REDIS_REPLY_NIL:
			outstr = "";
			result = false;
			break;
		case REDIS_REPLY_ERROR:
			outstr = reply->str;
			result = false;
			break;
		default:
			outstr = "";
			result = false;
			break;
		}
	}
	if (reply) {
		freeReplyObject(reply);
	}
	return result;
}

//not user para _context
redisReply* Redis::executeCmd(const char * command[], int num, size_t * vlen) {
	//in case redis disconnect and reconnect
	if (context_ == NULL) {
		if (-1 == connect(ip_, port_, auth_, index_)) {
			return NULL;
		}
	}
	redisReply* reply = NULL;
	reply = (redisReply*)redisCommandArgv(context_, num, command, vlen);
	if (NULL == reply)//�������صĴ���  ��Ҫ����
	{
		LOGE("redis: execute cmd fail,try reconnect");
		redisFree(context_);
		context_ = NULL;

		//if command exec fail,try exec again
		if (-1 != connect(ip_, port_, auth_, index_)) {
			reply = (redisReply*)redisCommandArgv(context_, num, command, vlen);
			if (NULL == reply)
			{
				LOGE("redis: try again execute cmd fail");
				redisFree(context_);
				context_ = NULL;
			}
		}
	}
	return reply;
}

redisReply * Redis::getReply()
{
	void *reply;
	if (context_->flags & REDIS_BLOCK) {
		if (redisGetReply(context_, &reply) != REDIS_OK)
			return NULL;
		return (redisReply*)reply;
	}
	return NULL;
}
