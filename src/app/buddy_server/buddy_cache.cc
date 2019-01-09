#include "buddy_cache.h"
#include <cstring>
#include <log_util.h>
#include <config_file_reader.h>


namespace buddy {
	static const int  db_index_buddy = 5;
	static const int  db_index_user_state = 8;

	BuddyCache::BuddyCache()
	{
	}

	int BuddyCache::init()
	{
		std::string redis_ip = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_IP);
		short redis_port = ConfigFileReader::getInstance()->ReadInt(CONF_REDIS_PORT);
		std::string auth = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_AUTH);
	
		if (user_state_redis_client_.connect(redis_ip, redis_port, auth, db_index_user_state) != 0) {
			LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
			return -1;
		}
		if (buddy_list_redis_client_.connect(redis_ip, redis_port, auth, db_index_buddy) != 0) {
			LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
			return -1;
		}

		return 0;
	}

	user_t * BuddyCache::getUser(const std::string&  user_id)
	{
		user_t* user = NULL;
		if (m_users.size() > 300000) {
			int count = 0;
			auto it = m_users.begin();
			while (it != m_users.end()) {
				
				delete it->second;
				++it;
				if (++count == 100000) {
					break;
				}
			}
		}
	
		user = new User(user_id);
		m_users[user_id] = user;
		return user;
	}

	user_t * BuddyCache::findUser(const std::string&  user_id)
	{
		auto it = m_users.find(user_id);
		if (it != m_users.end()) {
			return it->second;
		}
		return NULL;
	}

	user_t * BuddyCache::queryBuddyList(const std::string&  user_id)
	{
		user_t* user = findUser(user_id);
		if (!user) {
			user = getUser(user_id);
			if (!user) {
				LOGD("alloc memory fail");
				return NULL;
			}
			//first query user buddylist

			std::string state;
			const char *command[2];
			size_t vlen[2];

			std::string key = "ubl_";
			key += user_id;

			command[0] = "get";
			command[1] = key.c_str();

			vlen[0] = 3;
			vlen[1] = key.length();
			std::string result;
			bool res= buddy_list_redis_client_.query(command, sizeof(command) / sizeof(command[0]), vlen , result);
			if (res) {
				RDBuddyList buddylist;
				if (!buddylist.ParseFromArray(result.data(), result.length())) {
					LOGE("BuddyList parse fail");
					return NULL;
				}
				user->update_time = buddylist.update_time();
				user->buddy_list.clear();
				const auto& buddy_list = buddylist.buddy_info_list();
				auto it = buddy_list.begin();
				for (it; it != buddy_list.end(); it++) {
					user->addBuddy(it->user_id(), it->status());
				}
			}
			else {
				//saving offline user can reduce query num
				LOGD("query redis user[user_id:%s] buddy list fail",user_id.c_str());
			}
		}
		return user;
	}


	bool BuddyCache::getBuddyList(const std::string&  user_id, RDBuddyList& buddylist) {

		std::string state;
		const char *command[2];
		size_t vlen[2];

		std::string key = "ubl_";
		key += user_id;

		command[0] = "get";
		command[1] = key.c_str();

		vlen[0] = 3;
		vlen[1] = key.length();
		std::string result;

		bool res = buddy_list_redis_client_.query(command, sizeof(command) / sizeof(command[0]), vlen, result);
		if (res) {
			if (!buddylist.ParseFromArray(result.data(), result.length())) {
				LOGE("BuddyList parse fail");
				return false;
			}
			return true;
		}

		return false;
	}

	bool BuddyCache::getBuddyList(User* user) {

		RDBuddyList  buddylist;
		if (getBuddyList(user->user_id, buddylist)) {
			user->update_time = buddylist.update_time();
			user->buddy_list.clear();
			const auto& buddy_list = buddylist.buddy_info_list();
			auto it = buddy_list.begin();
			for (it; it != buddy_list.end(); it++) {
				user->addBuddy(it->user_id(), it->status());
			}
			return true;
		}
		return false;
	}

	bool BuddyCache::getBuddyList(User* user, RDBuddyList&  buddylist) {

		if (getBuddyList(user->user_id, buddylist)) {
			user->update_time = buddylist.update_time();
			user->buddy_list.clear();
			const auto& buddy_list = buddylist.buddy_info_list();
			auto it = buddy_list.begin();
			for (it; it != buddy_list.end(); it++) {
				user->addBuddy(it->user_id(), it->status());
			}
			return true;
		}
		return false;
	}
	bool BuddyCache::queryUserState(const std::string&  user_id,int& nid,int& fd)
	{
		const char *command[2];
		size_t vlen[2];

		std::string key = std::string("us_") + user_id;
		
		command[0] = "get";
		command[1] = key.c_str();

		vlen[0] = 3;
		vlen[1] = key.length();
		

		std::string state;
		bool res = user_state_redis_client_.query(command, sizeof(command) / sizeof(command[0]), vlen, state);
		if (res) {
			char str[128] = { 0 };
			char* pptr[5] = { 0 };
			int nid, device_type, version;

			strcpy(str, state.c_str());
			char* pos = str;
			//state format:  nid:%d_fd:%d_dt:%d_v:%d_ph:%s
			int i = 0;
			while (*pos) {
				if (*pos == ':') {
					pptr[i++] = pos + 1;
					if (i >= 4) {
						break;
					}
				}
				if (*pos == '_') {
					*pos = '\0';
				}
				++pos;
			}

			nid = atoi(pptr[0]);

			fd = atoi(pptr[1]);

			LOGD("query redis user[id:%s nid:%d] online--------------------------->", user_id.c_str(), nid);
			return true;
		}
		else {
			//saving offline user can reduce query num
			LOGD("query redis user[user_id:%s] offline", user_id.c_str());
		}

		return false;
	}

	User::User(const std::string&  id) :user_id(id),
		update_time(0) {}
	void User::addBuddy(const std::string user_id, int status)
	{
		Buddy* buddy = new Buddy;
		buddy->id = user_id;
		buddy->status = status;
		buddy_list.insert(buddy);
	}
	void User::clearBuddy()
	{
		auto it = buddy_list.begin();
		while (it != buddy_list.end()) {
			delete *it;
			++it;
		}
		buddy_list.clear();
	}
	int User::buddyRelation(const std::string & id)
	{
		auto it = buddy_list.begin();
		while (it != buddy_list.end()) {
			if ((*it)->id == id) {
				return (*it)->status;
			}
			++it;
		}
		return -1;
	}
}
