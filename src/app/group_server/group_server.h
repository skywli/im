#ifndef _CONNECTION_SERVER_H
#define _CONNECTION_SERVER_H

#include "IM.Buddy.pb.h"
#include "common/network/tcp_service.h"
#include <google/protobuf/message.h>
#include <list>
#include <string>
#include <pthread.h>
#include <atomic>
#include <lru.h>
#include <lock.h>
#include <thread_pool.h>
#include <msg_process.h>
#include <time_util.h>
#include <redis_client.h>
#include "group_cache.h"
#include <cnode.h>
#include "user_cache.h"

#include <IM.Redis.pb.h>
#include <IM.Group.pb.h>
#include "common/core/instance.h"
#define VERSION_0                   0    //sdk         
#define VERSION_1                   1    //bulik msg not ack
#define MSG_ACK_TIME                  6

#define  CHAT_MSG_THREAD_NUM         5
using namespace com::proto::basic;
using namespace com::proto::redis;
using namespace com::proto::group;
using namespace group;
class NodeConnectedState;

namespace group {
	class GroupServer;
	typedef bool (GroupServer::*handlerCb)(int sockfd, SPDUBase&  base);
	struct MHandler {
		int                fd;
		SPDUBase           spdu;
		//msg_handler        handler;
		TIME_T             time;
		handlerCb         handler;
	};

	typedef void (GroupServer::*Fnotify)(int, const std::string &, const google::protobuf::Message &, int);
	class LocationShareTask {
	private:
		class User {
		public:
			User() :answer_(false) {}
		public:
			int user_id_;
			std::string phone_;
			bool answer_;
		};
	public:
		LocationShareTask(uint64_t id, int user_id) :id_(id), invite_user_id_(user_id) {}
		~LocationShareTask();
		void addInviteUser(uint64_t user_id, const std::string phone);
		void addAnswerUser(uint64_t user_id);
		//void notifyUsers(GroupServer * msg_server, Fnotify cb, const google::protobuf::Message & _msg, int cmd);
		void notifyUsers(GroupServer* msg_server, Fnotify cb, const google::protobuf::Message &_msg, int cmd);
		int invitor() {
			return invite_user_id_;
		}
	private:
		uint64_t  id_;
		int    invite_user_id_;
		std::list<User*>  user_list_;

	};
	class GroupServer :public Instance {
	public:

		int sendConn(SPDUBase & base);

		static GroupServer* getInstance();
		int init();
		int initCmdTable();
		void start();

		void registCmd(int sid, int nid);

		virtual void onData(int _sockfd, PDUBase* _base);
		void onEvent(int _sockfd, ConnectionEvent event);

		int processInnerMsg(int sockfd, SPDUBase & base);

		static int innerMsgCb(int sockfd, SPDUBase & base, void * arg);

		static void Timer(int fd, short mask, void* privdata);
		void cron();
		struct CmdTable;
	public:
		
		void ResetPackBody(SPDUBase &_pack, google::protobuf::Message &_msg, int _commandid);

		void ReplyChatResult(int _sockfd, SPDUBase &_pack, ERRNO_CODE _code, bool is_target_online = false, uint64_t msg_id = 0);

		//msg
		bool ProcessIMChat_Group(int _sockfd, SPDUBase& _base);

	

		void ProcessIMChat_broadcast(int _sockfd, SPDUBase& _base);
		void ProcessBulletin_broadcast(int _sockfd, SPDUBase& _base);

		//regist callback
		static	void connectionStateEvent(NodeConnectedState* state, void * arg);
		static void configureStateCb(SPDUBase & base, void * arg);

		int configureState(SPDUBase & base);

		//locationshare

		void notifyUsers(int user_id, const std::string & ph, const google::protobuf::Message & _msg, int cmd);

	private:
		GroupServer();
		
		static void ProcessClientMsg(int _sockfd, SPDUBase*  _base);
		void UserstateBroadcast(SPDUBase & _base);
		//-1:fail 0:not group list 1:0k
		int getUserGroupList(user_t * user);

		bool getGroupInfo(uint32_t group_id, GroupInfo & group_info);

		bool getGroupInfo(uint32_t group_id);
		
		
		bool createGroupReq(int sockfd, SPDUBase & base);
	
		bool groupListReq(int sockfd, SPDUBase & base);
		bool groupListRsp(int sockfd, SPDUBase & base);
		bool groupInfoReq(int sockfd, SPDUBase & base);
		bool groupInfoRsp(int sockfd, SPDUBase & base);

		bool getGroupMember(uint32_t group_id, RDGroupMemberList & rd_member_list);

		bool getGroupMember(Group * pGroup);


		bool groupMemberListReq(int sockfd, SPDUBase & base);

		bool groupMemberListRsp(int sockfd, SPDUBase & base);
		bool createGroupRsp(int sockfd, SPDUBase & base);
		bool dissolveGroupReq(int sockfd, SPDUBase & base);
		bool dissolveGroupRsp(int sockfd, SPDUBase & base);
		bool groupInfoModifyReq(int sockfd, SPDUBase & base);
		bool groupInfoModifyRsp(int sockfd, SPDUBase & base);
		bool changeMemberReq(int sockfd, SPDUBase & base);
		bool changeMemberRsp(int sockfd, SPDUBase & base);
	
		bool AppendMsgIdtoRedis(const std::string & user_id, const std::string & id);
		bool ExecuteCmdRedis(int num);

		bool InsertMsgIdtoRedis(const std::string & user_id, const std::string & id);
	

		void groupmemberReq(uint32_t group_id, int sockfd, SPDUBase & spdu, handlerCb cb);
		bool queryGroupmemberRsp(int sockfd, SPDUBase & base);
		int handlerMsg(uint32_t group_id);
		bool groupSetReq(int sockfd, SPDUBase & base);
		bool groupSetRsp(int sockfd, SPDUBase & base);
		bool changeMemberTransferRsp(int sockfd, SPDUBase & base);
		bool changeMemberTransferReq(int sockfd, SPDUBase & base);
		uint64_t getMsgId();
		
	
		//storage msg
		void storage_common_msg(const std::string * data);

		void storage_apple_msg(const std::string* object);
		void storage_broadoffline_msg(std::string user, std::string msg_id);
			
			
	private:

		std::string                m_ip;
		short                      m_port;
	
		Redis                      storage_offline_msg_redis_;  //user offline msg
		Redis						group_redis_;
        Redis                       storage_msg_redis_;
		int                         m_node_id_;
		std::recursive_mutex        m_msgid_mutex_;
		int                         m_cur_index_;
		time_t                      m_lastTime;

		//stastic
		atomic_ullong               m_total_recv_pkt;

	
		ThreadPool*                 m_route_works;
		ThreadPool*                 m_storage_msg_works;


		MsgProcess                  m_chat_msg_process;

		//	MsgProcess                 m_common_msg_process;
		SdEventLoop*                m_loop;

		UserCache                  m_user_cache;

		CNode*                     m_pNode;

		std::map<uint64_t, LocationShareTask*>  m_localshare;

		int                       m_dbproxy_fd;
		GroupCache                m_group_cache;

		//use for query group member
		std::set<uint32_t>        m_queryMember;
		std::recursive_mutex       m_memberlock;

		std::recursive_mutex       m_msglock;
		std::map<uint32_t, std::list<MHandler*>> m_msgHandler;

		TcpService                 tcpService_;
};
}
#endif
