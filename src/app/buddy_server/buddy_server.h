#ifndef _CONNECTION_SERVER_H
#define _CONNECTION_SERVER_H

#include "common/network/tcp_service.h"
#include "common/core/instance.h"
#include "IM.Buddy.pb.h"
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
#include "buddy_cache.h"
#include <cnode.h>
#include <redis.h>
#define VERSION_0                   0    //sdk         
#define VERSION_1                   1    //bulik msg not ack
#define MSG_ACK_TIME                  6

#define  CHAT_MSG_THREAD_NUM         5
using namespace com::proto::basic;
using namespace com::proto::buddy;

class NodeConnectedState;

namespace buddy {

	class BuddyServer;
	typedef void (BuddyServer::*Fnotify)(int, const std::string &, const google::protobuf::Message &, int);
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
		//void notifyUsers(BuddyServer * msg_server, Fnotify cb, const google::protobuf::Message & _msg, int cmd);
		void notifyUsers(BuddyServer* msg_server, Fnotify cb, const google::protobuf::Message &_msg, int cmd);
		int invitor() {
			return invite_user_id_;
		}
	private:
		uint64_t  id_;
		int    invite_user_id_;
		std::list<User*>  user_list_;

	};
	class BuddyServer;
	typedef void (BuddyServer::*handlerCb)(int sockfd, SPDUBase&  base);
	struct MHandler {
		int                fd;
		SPDUBase           spdu;
		//msg_handler        handler;
		TIME_T             time;
		handlerCb         handler;
	};
	class BuddyServer :public Instance {
	public:

		static BuddyServer* getInstance();
		int init();
		void start();



		void registCmd(int sid, int nid);

		virtual void onData(int _sockfd, PDUBase* _base);
		virtual void onEvent(int fd, ConnectionEvent event);

		int processInnerMsg(int sockfd, SPDUBase & base);

		static int innerMsgCb(int sockfd, SPDUBase & base, void * arg);

		static void Timer(int fd, short mask, void* privdata);
		void cron();
	public:
		
		void ResetPackBody(SPDUBase &_pack, google::protobuf::Message &_msg, int _commandid);

		void ReplyChatResult(int _sockfd, SPDUBase &_pack, ERRNO_CODE _code, bool is_target_online = false, uint64_t msg_id = 0);

		//msg
		void ProcessIMChat_Personal(int _sockfd, SPDUBase& _base);

	

		void ProcessIMChat_broadcast(int _sockfd, SPDUBase& _base);
		void ProcessBulletin_broadcast(int _sockfd, SPDUBase& _base);

		//regist callback
		static	void connectionStateEvent(NodeConnectedState* state, void * arg);
		static void configureStateCb(SPDUBase & base, void * arg);

		int configureState(SPDUBase & base);

		//locationshare

		void notifyUsers(int user_id, const std::string & ph, const google::protobuf::Message & _msg, int cmd);

		void ProcessLocationShareInvite(int sockfd, SPDUBase& base);
		void ProcessLocationShareJoin(int sockfd, SPDUBase& base);
		void ProcessLocationShareQuit(int sockfd, SPDUBase& base);
		void ProcessLocationShareContinue(int sockfd, SPDUBase& base);

	private:
		BuddyServer();
		int sendDispatch(SPDUBase& base);
		int sendConn(SPDUBase & base);
		static void ProcessClientMsg(int _sockfd, SPDUBase*  _base);
		void orgListReq(int sockfd, SPDUBase & base);
		void orgListRsp(int sockfd, SPDUBase & base);
		bool getUserInfo(const std::string & user_id, UserInfo & user_ifno);
		bool delUserInfo(const std::string & user_id);
		bool setUserInfo(const std::string & user_id, const void * data, int len);
		void userInfoReq(int sockfd, SPDUBase & base);
		void userInfoRsp(int sockfd, SPDUBase & base);
		void userInfoOptReq(int sockfd, SPDUBase & base);
		void buddyReqListReq(int sockfd, SPDUBase & base);
		void buddyReqListRsp(int sockfd, SPDUBase & base);
		
		int handlerMsg(const std::string & user_id);

		void optBuddyReq(int sockfd, SPDUBase & base);
		void optBuddyRsp(int sockfd, SPDUBase & base);
		
		int msgNodeStateChange(int sockfd, int sid, int nid, int state);
		uint64_t getMsgId();

		bool InsertMsgIdtoRedis(const std::string & user_id, const std::string & id);

		

		static void check_msg(BuddyServer* s);
		
	
		//storage msg
		void storage_common_msg(const std::string * data);

		void storage_apple_msg(const std::string* object);
		void storage_broadoffline_msg(std::string user, std::string msg_id);


		//get buddylist 
		void buddyListReq(const std::string & user_id, int sockfd, SPDUBase & spdu, handlerCb cb);
		bool buddyListRsp(int sockfd, SPDUBase & base);
	private:

		std::string                m_ip;
		short                      m_port;
	
		//RedisClient redis_client;
		Redis                       storage_msg_redis_;

		Redis                       storage_offline_msg_redis_;

		Redis                       user_info_;

		int                         m_node_id_;
		std::recursive_mutex        m_msgid_mutex_;
		int                         m_cur_index_;
		time_t                      m_lastTime;

		//stastic
		atomic_ullong               m_total_recv_pkt;

	
		ThreadPool*                 m_storage_msg_works;


		MsgProcess                  m_chat_msg_process;

		//	MsgProcess                 m_common_msg_process;
		SdEventLoop*                loop_;
		TcpService                  tcpService_;

		BuddyCache                  m_buddy_cache;

		CNode*                     m_pNode;

		std::map<uint64_t, LocationShareTask*>  m_localshare;

		int                       m_dbproxy_fd;

		std::recursive_mutex       m_buddylistlock;

		map<std::string, list<MHandler*> > m_msgHandler;
	};
	
}
#endif
