#ifndef _CONNECTION_SERVER_H
#define _CONNECTION_SERVER_H

#include "tcp_service.h"
#include <list>
#include <string>
#include <pthread.h>
#include <atomic>
#include <lock.h>
#include <redis_client.h>
#include <state.h>
#include <IM.Login.pb.h>
#include <msg_process.h>
#include <user_state.h>
#include <lru.h>
#include <cnode.h>

using namespace com::proto::login;
#define VERSION_0                   0    //sdk         
#define VERSION_1                   1    //bulik msg not ack
#define MSG_ACK_TIME                  5

#define  CHAT_MSG_THREAD_NUM         5
#define  LOGIN_THREAD              3


class NodeConnectedState;

typedef Lru<std::string, User*> User_Map_t;
typedef std::unordered_map<int, std::list<std::string>> Node_User_list;
class LoginServer:public TcpService {


	//who connect me
private:
	class SNode {
	public:
		int fd;
		int sid;
		int nid;
	};
public:
	static LoginServer* getInstance();
	int init();
	void start();

	virtual void OnRecv(int _sockfd, PDUBase* _base);
    virtual void OnConn(int _sockfd);
    virtual void OnDisconn(int _sockfd);

private:
	
	LoginServer();
	~LoginServer();
	static void ProcessClientMsg(int _sockfd, SPDUBase*  _base);
	int processInnerMsg(int sockfd, SPDUBase & base);
	static int innerMsgCb(int sockfd, SPDUBase & base, void * arg);
	
	void ProcessHeartBeat(int _sockfd, SPDUBase& _pack);
	int BuildUserCacheInfo(User * user, SPDUBase & _base, User_Login & _login);

	void ProcessUserLogin(int _sockfd, SPDUBase&  _base);
	void ProcessUserLogout(int sockfd, SPDUBase & _base, bool except);
	void KickedNotify(User * user, int sockfd, std::string device_id, int device_type);
	
	static void Timer(int fd, short mask, void * privdata);
	
	void delConnection(int sockfd);
	User * getUser(const std::string & user_id);
	User * findUser(const std::string & user_id);
	
	static void configureStateCb(SPDUBase & base, void * arg);
	int configureState(SPDUBase & base);


	static void connectionStateEvent(NodeConnectedState* state, void* arg);
	void registCmd(int sid, int nid);
	
	int saveUserState(User* user);
	int deleteUserState(const std::string & user_id);

	void addConnection(int sockfd, int sid, int nid);
private:
	std::string                m_ip;
	short                      m_port;

	RedisClient                redis_client;
	User_Map_t                 m_user_map; //save user include offline 
	std::recursive_mutex       m_user_map_mutex;

	Node_User_list             m_node_user_list;
	std::recursive_mutex       m_node_userid_mutex;

	int                        m_logins; //login users num; 
	
	MsgProcess                 m_process[LOGIN_THREAD];
	StateService               m_stateService;
	SdEventLoop*               m_loop;

	atomic_int                 m_onliners;
	

	std::list<SNode*>          m_nodes;

	CNode*                      m_pNode;
	//process msg thread
};

#endif
