#pragma once
#include <unordered_map>
#include<redis_client.h>
#include <redis.h>
#include <IM.Basic.pb.h>
using namespace com::proto::basic;
namespace msg_server {
	typedef struct User {
		User();
		User(const std::string& id);
		int clear();
		const char* state() {
			const char* info = "";
			if (online == USER_STATE_ONLINE) {
				info = "online";
			}
			else if (online == USER_STATE_OFFLINE) {
				info = "offline";
			}
			else {
				info = "logout";
			}
			return info;
		}
		std::string  user_id;
		int  nid;
		int  fd;
		int  version;
		char device_type;
		int online;

		time_t online_time;
		time_t ack_time;
		time_t send_time;
		int send_pending;
		bool has_offline_msg;//reduce query offline num

	}user_t;
	typedef std::unordered_map<std::string, user_t*> User_Map_t;

	class UserCache {
	public:
		UserCache();
		int init();
		//create a user
		user_t* getUser(const std::string&  user_id);
		user_t* findUser(const std::string&  user_id);

		user_t* queryUser(const std::string&  user_id);
	private:

		User_Map_t        m_users;
		Redis             user_state_redis_client_;
	};
}