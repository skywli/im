#ifndef _LOADBALANCE_H
#define _LOADBALANCE_H


#include "common/network/tcp_service.h"
#include "common/core/instance.h"
#include <list>
#include <mutex>

#include "typedef.h"
#include<cnode.h>
#include <google/protobuf/message.h>

class LoadbalanceObject;
typedef std::list<LoadbalanceObject> Loadbalance_List_t;


#define     ROUTE_STATE_OK               0
#define     ROUTE_STATE_ERR              1
class LoadbalanceObject {
public:
	LoadbalanceObject();

	// 当前负载的ip和端口
	std::string ip_;
	int port_;

	// 当前负载的用户数量
	int current_balance_user_num_;

	// 当前负载的注册时间
	int regist_timestamp_;

	Socketfd_t sockfd_;
	int        id_;
	int state;
	bool operator > (const LoadbalanceObject &_lbo) {
		return current_balance_user_num_ > _lbo.current_balance_user_num_;
	}

	bool operator < (const LoadbalanceObject &_lbo) {
		return current_balance_user_num_ < _lbo.current_balance_user_num_;
	}

	bool operator()(const LoadbalanceObject &lhs, const LoadbalanceObject& rhs) const {
		return lhs.current_balance_user_num_ < rhs.current_balance_user_num_;
	}

	bool operator == (const LoadbalanceObject &_lbo) const {
		return _lbo.sockfd_ == sockfd_;
	}

};


class LoadBalanceServer :public Instance {
public:
	LoadBalanceServer();

	int init();
	int start();
	int innerMsgCb(int sockfd, SPDUBase & base, void * arg);
	virtual void onData(int _sockfd, PDUBase* _base);
	virtual void onEvent(int fd, ConnectionEvent event);


	void ProcessRegistService(int _sockfd, SPDUBase &_pack);
	void ProcessReportOnliners(int _sockfd, SPDUBase &_pack);
	void AllocateLoadbalance(std::string &_ip, short &_port, const std::string _userid);

	void ServiceInfo();

private:

	bool find_loadbalance_from_list(int _sockfd, std::string &_ip, int &_port);
	bool delete_loadbalance_from_list(int _sockfd);
	bool update_loadbalance_from_list(int _sockfd);
	LoadbalanceObject* get_loadbalance_less();
public:

	std::recursive_mutex loadbalance_mutex_;
	Loadbalance_List_t loadbalance_list_;
	int                index_;
	std::string           m_ip;
	short                 m_port;
	SdEventLoop*                        loop_;
	TcpService                         tcpService_;
	CNode*                    m_pNode;
};



#endif
