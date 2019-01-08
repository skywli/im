#ifndef _CONNECTION_SERVER_H
#define _CONNECTION_SERVER_H

#include "IM.Login.pb.h"
#include "tcp_service.h"
#include "block_tcp_client.h"
#include <google/protobuf/message.h>
#include <list>
#include <string>
#include <pthread.h>
#include <atomic>
#include <lru.h>
#include <lock.h>
#include <ThreadPool.h>
#include <msgProcess.h>
#include <time_util.h>
#include <redis_client.h>
#include <user_cache.h>
#include <statistics.h>
#include <CNode.h>
#include <leveldb/db.h>
#include <redis.h>
#define VERSION_0                   0    //sdk         
#define VERSION_1                   1    //bulik msg not ack
#define MSG_ACK_TIME                  6

#define  CHAT_MSG_THREAD_NUM         5
using namespace com::proto::basic;
using namespace com::proto::login;

class NodeConnectedState;
typedef std::unordered_map<std::string, int> Phone_userid_t;
namespace msg_server {
	class Ackmsg {
	public:
		Ackmsg(const std::string& user_id,uint64_t msg_id_) :user_id(user_id),msg_id(msg_id_) {
			ms = get_mstime();
		}
		std::string  user_id;
		uint64_t    msg_id;
		long long    ms;
	};

	class MsgServer;
	typedef void (MsgServer::*Fnotify)(int, const std::string &, const google::protobuf::Message &, int);
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
		//void notifyUsers(MsgServer * msg_server, Fnotify cb, const google::protobuf::Message & _msg, int cmd);
		void notifyUsers(MsgServer* msg_server, Fnotify cb, const google::protobuf::Message &_msg, int cmd);
		int invitor() {
			return invite_user_id_;
		}
	private:
		uint64_t  id_;
		int    invite_user_id_;
		std::list<User*>  user_list_;

	};
	class MsgServer :public TcpService {
	public:

		static MsgServer* getInstance();
		int init();
		void start();



		void registCmd(int sid, int nid);

		virtual void OnRecv(int _sockfd, PDUBase* _base);
		virtual void OnConn(int _sockfd);
		virtual void OnDisconn(int _sockfd);


		int processInnerMsg(int sockfd, SPDUBase & base);

		static int innerMsgCb(int sockfd, SPDUBase & base, void * arg);


		void ProcessIMChat_fromRoute(SPDUBase & base, uint64_t msg_id, user_t * user);

		void reportOnliners();

		static void Timer(int fd, short mask, void* privdata);
		void cron();
	public:
		void ConsumeHistoryMessage(const std::string& user_id);
		void ResetPackBody(SPDUBase &_pack, const google::protobuf::Message &_msg, int _commandid);

		void ReplyChatResult(int _sockfd, SPDUBase &_pack, ERRNO_CODE _code, bool is_target_online = false, uint64_t msg_id = 0);

		//msg
		void ProcessPushMsg(int _sockfd, SPDUBase& _base);

	
		void recordDeleteMsg(Ackmsg * msg);

		void clearRecordAckMsg();

		void ProcessPushMsg_ack(int _sockfd, SPDUBase & _base);

		void ProcessIMChat_broadcast(int _sockfd, SPDUBase& _base);
		void ProcessBulletin_broadcast(int _sockfd, SPDUBase& _base);

		void ProcessUserstateBroadcast(SPDUBase& _base);

		int PhoneQueryUserId(std::string _phone, int& _userid);

		//regist callback
		static	void connectionStateEvent(NodeConnectedState* state, void * arg);
		static void configureStateCb(SPDUBase & base, void * arg);

		int configureState(SPDUBase & base);




	private:
		MsgServer();
		int sendConn(SPDUBase & base);
		
		static void ProcessClientMsg(int _sockfd, SPDUBase*  _base);

		int saveChatMsg(SPDUBase & base, uint64_t msg_id);

		int getChatMsg(SPDUBase & base, uint64_t msg_id);

		int msgNodeStateChange(int sockfd, int sid, int nid, int state);
		uint64_t getMsgId();

		bool need_send_msg(user_t* user, SPDUBase & _base, uint64_t msg_id);

		void record_waiting_ackmsg(const std::string & user_id, uint64_t msg_id);

		void delete_ack_msg(const std::string & user_id, uint64_t msg_id);

		static void check_msg(MsgServer* s);
		void check_send_msg();

		//handler ack timeout 
		void ack_timeout_handler(const std::string & user_id);

	//user offline
		bool DeleteMsgIdtoRedis(const std::string & user_id, uint64_t msg_id);
		bool GetOfflineMsgIds(const std::string & user_id, std::list<std::string>& id_list);

		bool AppendDeleteMsgIdtoRedis(const std::string & user_id, uint64_t msg_id);

		bool ExecuteCmdRedis(int num);

		void recordAckMsg(Ackmsg * msg);



	private:

		std::string                m_ip;
		short                      m_port;
		int        ack_time_;
	
		Redis         storage_offline_msg_redis_;  //user offline msg
        Redis         pipeline_clear_offline_msg_redis_;
		User_Map_t  m_online_users;
		std::recursive_mutex user_map_mutex_;

		User_Map_t  m_offline_users;


		//send msg
		std::map<std::string, std::list<Ackmsg*>>  m_send_msg_map_;
		//std::recursive_mutex               m_send_msg_mutex_;
		//CRWLock                            m_rwlock_;

		//wait ack userid
		Lru<std::string, int>         m_ack_msg_map_;
		std::recursive_mutex        m_ack_msg_mutex_;

		int                         m_node_id_;
		std::recursive_mutex        m_msgid_mutex_;
		int                         m_cur_index_;
		time_t                      m_lastTime;

		//stastic
		atomic_ullong               m_total_recv_pkt;

		ThreadPool*                 m_storage_msg_works;
		ThreadPool*                 m_route_works;



		MsgProcess                  m_chat_msg_process;

		//	MsgProcess                 m_common_msg_process;
		SdEventLoop*                m_loop;

		UserCache                  m_user_cache;

		Statistics                 m_statistics;
		CNode*                     m_pNode;

		std::map<uint64_t, LocationShareTask*>  m_localshare;

		leveldb::DB*              m_db;

		//clear msg
		std::recursive_mutex        m_msg_clear_mutex_;
		std::list<Ackmsg*>         m_ackmsg_list;

		std::recursive_mutex        m_msg_count_mutex_;
		std::map<uint64_t, int>       m_msg_count_;
		int                         m_mq_fd;

	};
}
#endif
