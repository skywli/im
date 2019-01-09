#include "user_cache.h"
#include <cstring>
#include <log_util.h>
#include <config_file_reader.h>
namespace group {
	static const int db_index_user_state = 8;
	UserCache::UserCache()
	{
	}

	int UserCache::init()
	{
		std::string redis_ip = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_IP);
		short redis_port = ConfigFileReader::getInstance()->ReadInt(CONF_REDIS_PORT);
		std::string auth = ConfigFileReader::getInstance()->ReadString(CONF_REDIS_AUTH);

		if (user_state_redis_client_.connect(redis_ip, redis_port, auth, db_index_user_state) != 0) {
			LOGE("redis: connect redis ip:%s,port:%d fail", redis_ip.c_str(), redis_port);
			return -1;
		}

		return 0;
	}

	user_t * UserCache::getUser(const std::string&  user_id)
	{
		user_t* user = NULL;
		if (m_users.size() > 3000000) {
			auto it = m_users.begin();
			while (it != m_users.end()) {
				if (!it->second->online) {
					user = it->second;
					memset(user, 0, sizeof(user_t));
					user->user_id = user_id;

					m_users.erase(it);
					break;
				}
				++it;
			}
		}
		else {
			user = new User(user_id);
		}
		if (user) {
			m_users[user_id] = user;
		}
		return user;
	}

	user_t * UserCache::findUser(const std::string&  user_id)
	{
		auto it = m_users.find(user_id);
		if (it != m_users.end()) {
			return it->second;
		}
		return NULL;
	}

	user_t * UserCache::queryUser(const std::string&  user_id)
	{
		user_t* user = findUser(user_id);
		if (!user) {
			user = getUser(user_id);
			if (!user) {
				LOGD("alloc memory fail");
				return NULL;
			}
		}
		user->online = false;

		std::string key = std::string("us_") + user_id;

		const char *command[2];
		size_t vlen[2];

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

			user->nid = atoi(pptr[0]);
			user->online = true;
			user->fd = atoi(pptr[1]);
			user->device_type = atoi(pptr[2]);
			user->version = atoi(pptr[3]);

			LOGD("query redis user[id:%s nid:%d] online--------------------------->", user->user_id.c_str(), user->nid);
		}
		else {
			//saving offline user can reduce query num
			LOGD("query redis user[user_id:%s] offline", user_id.c_str());
		}

		return user;
	}

	User::User(const std::string&  id)
	{
		user_id = id;
		nid = 0;
		fd = 0;
		has_offline_msg = true;
		online = false;

		online_time = 0;
	
	}
}

