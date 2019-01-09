
#ifndef _CONNECTION_SERVICE_H
#define _CONNECTION_SERVICE_H

#include <list>
#include <string>
#include <msg_service.h>
#include <tcp_service.h>
#include <node_mgr.h>


#define     CLIENT_ONLINE       1
#define     CLIENT_OFFLINE      2


class ConnService:public TcpService  {
public:
	ConnService();
	int init();
	int start();
	int getNodeId();
	void setNodeId(int node_id);

	int recvBusiMsg(int sockfd, PDUBase& _data);
	virtual void OnRecv(int _sockfd, PDUBase* _base);
	virtual void OnConn(int _sockfd);
	virtual void OnDisconn(int _sockfd);
	void parse(Connection * conn);
	int deleteClient(int _user_id);

private:
	
	static void Timer(int fd, short mask, void * privdata);
	void reportOnliners();
	void count(int cmd);
	void statistic();
	class Client {

	public:
		Client():t(0), state(CLIENT_OFFLINE){  }
		std::string user_id;
		long long t;
		int state;
	};

private:
	// 本服务器的ip和端口
	std::string    m_ip;
	int            m_port;
	SdEventLoop*   m_loop;
	MsgService                 m_msgService;

	int                        m_max_conn;
	
	Client*                    m_clients;
	

	//use for stastic

	long long                  m_conns; //连接数
	std::map<int, long long>     m_pkts;
	long long                   m_total_pkts;
	long long                   m_total_chat_pkts;
//	NodeMgr                    m_nodeMgr;

};

#endif
