#pragma once
#include "tcp_server.h"
#include <IM.Basic.pb.h>
#include <IM.Login.pb.h>
#include <IM.Msg.pb.h>
#include <map>
using namespace std;
using namespace com::proto::basic;
using namespace com::proto::login;
using namespace com::proto::msg;
struct User {
	int id;
	string msisdn;
	string passwd;
	string name;
	string sessid;
};
class IMClient :public TcpServer {
public:
	IMClient();
	~IMClient();
	virtual void OnRecv(int _sockfd, PDUBase &_base);
	virtual void OnRecv(int _sockfd, PDUBase* _base) ;
	virtual void OnConn(int _sockfd);
	virtual void OnDisconn(int _sockfd);
	virtual void OnSendFailed(PDUBase &_data);
	void loginAck(PDUBase & _base);
	void chatMsg(int _sockfd,PDUBase& _base);
    void bulletin(int sockfd_,PDUBase& _base);
	void OnLogin(int _sockfd);
	int getRecvPkt();
	int getLogin();
    void init();
	int createGroup(std::vector<std::string> members, std::string owner,char* ip,int port,char* auth);

	int createGroup();
private:
	int   m_num;
	int    m_login;
	int total_recv_pkts;
	int sec_recv_pkts;
    map<int,string> m_sock_userid;

};
