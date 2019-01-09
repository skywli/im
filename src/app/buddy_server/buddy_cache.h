#pragma once
#include <unordered_map>
#include <set>
#include <redis.h>
#include <IM.Redis.pb.h>
using  com::proto::redis::RDBuddyList;
namespace buddy {
	enum BuddyStatus {
		//status_Req=0,
		status_delete = 0,
		status_buddy=1,
		status_black=2
	};
	struct Buddy {
		std::string  id;
		int          status;
	};
	typedef struct User {
		User();
		User(const std::string& id);
		void addBuddy(const std::string user_id, int status);
		void clearBuddy();
		int buddyRelation(const std::string& id);
		std::string  user_id;
		uint64_t update_time;
		std::set<Buddy*> buddy_list;
	}user_t;
	typedef std::unordered_map<std::string, user_t*> User_Map_t;


	class BuddyCache {
	public:
		BuddyCache();
		int init();
		//create a user
		user_t* getUser(const std::string&  user_id);
		user_t* findUser(const std::string&  user_id);
		

		user_t* queryBuddyList(const std::string&  user_id);
	
		bool getBuddyList(User * user);
		bool getBuddyList(User * user, RDBuddyList & buddy_list);
	
		bool queryUserState(const std::string & user_id, int & nid, int & fd);
		
	//	user_t* queryBuddyList(const std::string&  user_id,);
	private:
		bool getBuddyList(const std::string & user_id, RDBuddyList & buddy_list);
		User_Map_t        m_users;

		Redis             user_state_redis_client_;
		Redis             buddy_list_redis_client_;
	};
}