#ifndef _GroupCache_H_
#define _GroupCache_H_

#include <map>
#include <list>
#include <set>
#include <lock.h>
#include <lru.h>
#include <time_util.h>

namespace group{
	typedef std::string  UserId_t;
	typedef uint32_t    GroupId_t;
	class Group;
	class GroupMsgHandler;
	
	enum GroupRole {
		GROUP_OWNER = 0,
		GROUP_ADMIN = 1,
		GROUP_COMMON = 2
	};

	enum UserState {
		OFFLINE = 0,
		ONLINE = 1,
		LEAVE = 3
	};

	typedef struct connaddr {
		int sock_id;
		int srv_id;
	}connaddr_t;

	struct user_info
	{ 
		int                    tag;                                                              //group list是否完整 1：非完整 
	};

	struct Member {
		Member(const UserId_t& id,uint32_t r):user_id(id),role(r){}
		UserId_t     user_id;
		uint32_t     role : 8;
		//user_t*      user_info;      //用户详细信息
	};

	//不包含orgnode
	class Group {
	public:
		~Group();
		Group(const GroupId_t& group_id);
		Member* addMember(const UserId_t& user_id, uint32_t role);
		int delMember(const  UserId_t& user_id);
		const list< Member*>& getMemberList() const;
		const list<Member*>&  getAdinList() const;
	
		uint32_t   getGroupId();
	
		void       clear();

		void       setGroupInfoUpdateTime(uint64_t time);
		uint64_t   getGroupInfoUpdateTime();
		void       setMemberUpdateTime(uint64_t time);
		uint64_t   getMemberUpdateTime();

		bool empty();
		bool dissolve();
		int size();
		bool hasMember(const UserId_t& id);
		void setDissolve();

	private:
		int               m_nums;//成员数
		GroupId_t          m_groupid;
		list<Member*>     m_memberList;
		list<Member*>     m_adminList;

		bool              m_bDissolve;
		uint64_t          m_groupInfoUpdateTime;
		uint64_t          m_memberUpdateTime;

	
	public:
		uint32_t          m_pkts;
		TIME_T            m_expireTime;
	};

	class GroupCache {
	public:

		GroupCache();
		~GroupCache();
		Group* createGroup();
		int DissolveGroup(const GroupId_t& group_id);
		Group* getGroup( const GroupId_t& group_id);

		Group* findGroup(const GroupId_t& group_id);

		int addGroup(Group* pGroup);

		/**
		* 缓存没有group 的消息
		*/
		

		void checkCache();

		Group * createGroup(const GroupId_t & id);

		static void check_cache();
	private:
		uint32_t  getGroupId();
	private:
		Lru<GroupId_t, Group*>                m_grouplist;
	

		map<uint32_t, Group*>                m_exprieGroup;
		CLock                                m_grouplock;

		CRWLock                              m_msglock;
	};

}

#endif
