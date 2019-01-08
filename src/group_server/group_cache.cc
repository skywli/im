
#include "group_cache.h"
#include <cstring>
#include <log_util.h>

namespace group {
	CRWLock memlock;
	static uint64_t                             s_usrmemory;
	static uint64_t                             s_maxmemory = 100 * 1024 * 1024;
	int       g_nodeExpireTime = 5 * 1000;//ms

	static int       s_msgExpireTime = 5 * 1000;//ms

#define MAX_GROUP_NUMBER    100000

	GroupCache g_GroupCache;


	void  GroupCache::check_cache() {
		g_GroupCache.checkCache();
	}

	uint32_t GroupCache::getGroupId()
	{
		return uint32_t();
	}

	GroupCache::GroupCache()
	{

	}

	GroupCache::~GroupCache()
	{
	}

	void GroupCache::checkCache()
	{
		Group* pGroup;
		{
			m_grouplock.lock();
			//Çå³ý¹ýÆÚgroup 
			map<uint32_t, Group*>::const_iterator it = m_exprieGroup.begin();
			for (it; it != m_exprieGroup.end();) {
				pGroup = it->second;

				TIME_T now;
				getCurTime(&now);
				if (interval(pGroup->m_expireTime, now) > g_nodeExpireTime) {
					delete pGroup;
					m_exprieGroup.erase(it++);
				}
			}
			//ÌÔÌ­recently latest  20% group
			if (m_grouplist.size() > MAX_GROUP_NUMBER) {
				int num = (m_grouplist.size() / 100) * 20;
				for (int i = num; i > 0 && (!m_grouplist.empty()); i--) {
					pGroup = m_grouplist.back().second;
					GroupId_t group_id = pGroup->getGroupId();
					m_grouplist.pop();

					getCurTime(&pGroup->m_expireTime);
					m_exprieGroup[group_id] = pGroup;
				}
			}
			m_grouplock.unlock();
		}
	}

	Group* GroupCache::createGroup(const GroupId_t & id)
	{
		Group* pGroup = new Group(id);
		m_grouplist.push(id, pGroup);
		return pGroup;
	}

	int GroupCache::DissolveGroup(const GroupId_t & group_id)
	{
		Lru<GroupId_t, Group*>::iterator it = m_grouplist.find(group_id);
		if (it != m_grouplist.end()) {
			Group* pGroup = it->second;
			m_grouplist.erase(it);
			delete pGroup;
		}
		return 0;
	}

	Group * GroupCache::findGroup(const GroupId_t & group_id)
	{
		Group* pGroup = NULL;

		Lru<GroupId_t, Group*>::iterator it = m_grouplist.find(group_id);
		do {
			if (it != m_grouplist.end()) {
				pGroup = it->second;
				break;
			}
			m_grouplock.lock();
			//query from expire group
			map<uint32_t, Group*>::const_iterator it = m_exprieGroup.find(group_id);
			if (it != m_exprieGroup.end()) {
				pGroup = it->second;
				m_grouplist.push(it->first, pGroup);
				m_exprieGroup.erase(it);
			}
			m_grouplock.unlock();
		} while (0);

		return pGroup;
	}

	int GroupCache::addGroup(Group * pGroup)
	{
		uint64_t combine_id = pGroup->getGroupId();
		m_grouplist.push(combine_id, pGroup);
		return 0;
	}


	Group::~Group()
	{
		m_adminList.clear();
		list<Member*>::const_iterator it = m_memberList.begin();
		for (it; it != m_memberList.end(); ++it) {
			delete *it ;
		}
	}

	Group::Group(const GroupId_t& group_id)
	{
		m_groupid = group_id;
		m_bDissolve = false;
		m_groupInfoUpdateTime = 0;
		m_memberUpdateTime = 0;
		m_nums = 0;
	
	}

	Member* Group::addMember(const UserId_t& user_id, uint32_t role)
	{
		auto it = m_memberList.begin();
		for (it; it != m_memberList.end(); it++) {
			if ((*it)->user_id == user_id) {
				return *it;
			}
		}
		Member* mem = new Member(user_id,role);
	
		m_memberList.push_back(mem);
		if (role == GROUP_ADMIN || role == GROUP_OWNER) {
			m_adminList.push_back(mem);
		}

		return mem;
	}

	int Group::delMember(const UserId_t&  user_id)
	{
		list<Member*>::iterator it = m_memberList.begin();
		while (it != m_memberList.end()) {
			if ((*it)->user_id == user_id ) {
				Member* mem = *it;
				m_memberList.erase(it);
				//admin
				if (GROUP_COMMON != mem->role) {
					list<Member*>::iterator it = m_adminList.begin();
					while (it != m_adminList.end()) {
						if ((*it)->user_id == user_id) {
							m_adminList.erase(it);
							break;
						}
						++it;
					}
				}
				delete mem;
				break;
			}
			++it;
		}
		
		return 0;
	}

	void Group::clear()
	{
		m_adminList.clear();
		list<Member*>::const_iterator it = m_memberList.begin();
		for (it; it != m_memberList.end(); ++it) {
			delete *it;
		}
		m_memberList.clear();
		m_adminList.clear();
		
	}

	void Group::setGroupInfoUpdateTime(uint64_t time)
	{
		m_groupInfoUpdateTime = time;
	}

	uint64_t Group::getGroupInfoUpdateTime()
	{
		return m_groupInfoUpdateTime;
	}

	void Group::setMemberUpdateTime(uint64_t time)
	{
		m_memberUpdateTime = time;
	}

	uint64_t Group::getMemberUpdateTime()
	{
		return m_memberUpdateTime;
	}

	bool Group::empty()
	{
		return m_memberList.empty();
	}
	bool Group::dissolve() {
		return m_bDissolve;
	}

	int Group::size()
	{
		 return m_memberList.size();
	}

	bool Group::hasMember(const UserId_t & user_id)
	{
		list<Member*>::iterator it = m_memberList.begin();
		while (it != m_memberList.end()) {
			if ((*it)->user_id == user_id) {
				return true;
			}
            ++it;
		}
		return false;
	}

	void Group::setDissolve()
	{
		m_bDissolve = true;
	}

	const list<Member*>& Group::getMemberList()const
	{
		return m_memberList;
	}

	const list<Member*>& Group::getAdinList()const
	{
		return m_adminList;
	}


	GroupId_t Group::getGroupId()
	{
		return m_groupid;
	}
	

}

