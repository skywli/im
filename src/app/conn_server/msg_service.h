
#ifndef _MSG_SERVICE_H
#define _MSG_SERVICE_H

#include <list>
#include <string>
#include <tcp_service.h>
#include <cnode.h>

class ConnService;
struct NodeConnectedState;
class MsgService :public TcpService {
private:
	class SNode {
	public:
		SNode() {
			connected = false;
		}
		int sockfd;
		int nid;
		int sid;
		bool connected;
	};
public:
	MsgService();
	static void OnAccept(SdEventLoop * eventLoop, int fd, void * clientData, int mask);
	int init(SdEventLoop * loop, ConnService * conn);
	int start();

	
	static void connectionStateEventCb(NodeConnectedState* state, void * arg);
	void registLoadBalance(int sockfd);

	 void connectionStateEvent(int sockfd, int state, int sid, int nid);

	int  recvClientMsg(int _sockfd,  PDUBase& _base);
	int  processInnerMsg(int sockfd, SPDUBase& base);
	int randomGetSock();
	virtual void OnRecv(int _sockfd, PDUBase* _base);
	virtual void OnConn(int _sockfd);
	virtual void OnDisconn(int _sockfd);
	

	// 本服务器的ip和端口
	std::string m_ip;
	int        m_port;
	

	SdEventLoop*               m_loop;
	ConnService*               m_connService;

	std::list<SNode*>          m_nodes;
	int                       m_dispatch_num;

	int                       m_cur_dispatch;
	CNode*                    m_pNode;
};

#endif
