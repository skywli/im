#include "conn_service.h"
#include "log_util.h"
#include "config_file_reader.h"
#include <node_mgr.h>
#include "IM.Loadbalance.pb.h"
#include <IM.Login.pb.h>
#include <IM.Basic.pb.h>
#include <algorithm>
#include <arpa/inet.h>
#include <time_util.h>
using namespace com::proto::loadbalance;
using namespace com::proto::login;
using namespace com::proto::basic;
static int maxclients = 500000;
static int sec_recv_pkt;
static int total_recv_pkt;

static int sec_send_pkt;
int total_send_pkts;
int total_send_pkt;

#define CONFIG_FDSET_INCR  128
#define CONF_CONN_LISTEN_IP  "listen_ip"
#define CONF_CONN_LISTEN_PORT "listen_port"
ConnService::ConnService(){
	m_conns = 0;
	m_total_pkts = 0;
	m_total_chat_pkts=0;
}

int ConnService::init()
{
	m_ip = ConfigFileReader::getInstance()->ReadString(CONF_CONN_LISTEN_IP);
	m_port = ConfigFileReader::getInstance()->ReadInt(CONF_CONN_LISTEN_PORT);
	if (m_ip == "" || m_port == 0) {
		LOGE("not config out ip or port");
		return -1;
	}
	
	m_max_conn = maxclients + CONFIG_FDSET_INCR;
	m_clients = new Client[sizeof(Client)*(m_max_conn+1)];
	m_loop = getEventLoop();
	m_loop->init(m_max_conn);
	TcpService::init(m_loop);
	CreateTimer(5000, Timer, this);
	return m_msgService.init(m_loop, this);
}

int ConnService::start() {
	LOGD("connect server listen on %s:%d", m_ip.c_str(), m_port);
	if (TcpService::StartServer(m_ip, m_port) == -1) {
		LOGE("listen [ip:%s,port:%d]fail", m_ip.c_str(), m_port);
		return -1;
	}
	if (m_msgService.start() == -1) {
		return -1;
	}
	PollStart();
	return 0;
}

int ConnService::getNodeId()
{
	return ConfigFileReader::getInstance()->ReadInt(CONF_NODE_ID);
}


int ConnService::recvBusiMsg(int sockfd, PDUBase & _data)
{
	if (sockfd > m_max_conn) {
		return -1;
	}
	std::string user_id(_data.terminal_token, sizeof(_data.terminal_token));
	Client* client = &m_clients[sockfd];
	if (strcmp(client->user_id.c_str(), user_id.c_str())) {
		LOGE("sockfd[%d] is not the user[%s],belong to(%s)", sockfd, user_id.c_str(),client->user_id.c_str());
		return -1;

	}
	int cmd = _data.command_id;
	
	if (Send(sockfd, _data) == -1) {
		LOGE("send to user[%s] fail", user_id.c_str());
	}
	++total_send_pkts;
	if (cmd == USER_LOGIN_ACK) {
		User_Login_Ack ack;
		if (!ack.ParseFromArray(_data.body.get(), _data.length)) {
			LOGERROR(_data.command_id, _data.seq_id, "User_Login_Ack包解析错误");
			return 0;
		}
		if (ack.errer_no() == ERRNO_CODE_OK) {
			Client* client = &m_clients[sockfd];
			client->state = CLIENT_ONLINE;
			//TODO  couldn't close user socket
		}
	}
	count(cmd);
	return 0;
}

void ConnService::OnRecv(int _sockfd, PDUBase* _base) {
	if (_sockfd > m_max_conn) {
		LOGW("too much connection");
		CloseFd(_sockfd);
		return;
	}
	Client* client = &m_clients[_sockfd];
	std::string user_id=std::string(_base->terminal_token, sizeof(_base->terminal_token));
	if (user_id[0] == 0) {
		LOGE(" msg header not include user_id");
		delete _base;
		return;
	}
	if (client->state == CLIENT_OFFLINE && _base->command_id!=USER_LOGIN) {
		if (get_mstime() - client->t > 5) {
			LOGE("user(%s) offline", user_id.c_str());
			User_Offline offline;
			_base->ResetPackBody(offline, USER_LOGOFF);
			Send(_sockfd, *_base);
			delete _base;
			return;
		}
	}
	client->user_id = user_id;
	//client->app_id = std::string(_base->app_id, sizeof(_base->app_id));
	LOGD("recv user(%s) msg cmd(%d)", client->user_id.c_str(),_base->command_id);
	m_msgService.recvClientMsg(_sockfd, *_base);
	++total_recv_pkt;
	delete _base;
}

void ConnService::OnConn(int _sockfd) {
	if (_sockfd > m_max_conn) {
		LOGW("too much connection");
		CloseFd(_sockfd);
		return;
	}
   // LOGD("sockfd:%d connect",_sockfd);
	Client* client = &m_clients[_sockfd];
	client->t = get_mstime();
	m_conns++;
}

void ConnService::OnDisconn(int _sockfd) {
	if (_sockfd > m_max_conn) {
		return;
	}
	Client* client = &m_clients[_sockfd];
	client->t = 0;
	--m_conns;
    CloseFd(_sockfd);
	if (client->state == CLIENT_OFFLINE) {
		return;
	}
	client->state = CLIENT_OFFLINE;
	LOGT("user[userid:%s,fd:%d] disconnect", client->user_id.c_str(), _sockfd);
	User_LogOff log_off;
	log_off.set_user_id(client->user_id);
	
	PDUBase pack;
	std::shared_ptr<char> body(new char[log_off.ByteSize()], carray_deleter);
	log_off.SerializeToArray(body.get(), log_off.ByteSize());
	pack.body = body;
	pack.service_id = SID_LOGIN;
	pack.command_id = CID_USER_CONNECT_EXCEPT;
	pack.length = log_off.ByteSize();
	pack.seq_id = 0;
	memcpy(pack.terminal_token, client->user_id.c_str(), sizeof(pack.terminal_token));
	m_msgService.recvClientMsg(_sockfd, pack);
	
	//deleteClient(client->user_id);
}

void ConnService::parse(Connection* conn) {
	int fd = conn->fd;
	char* data = conn->buf;
	while (conn->buf_len >= HEAD_LEN) {
		//没有不完整的包
		if (!conn->less_pkt_len) {
			int* startflag = (int*)(data);
			if (ntohl(*(startflag)) == PDUBase::startflag)//正常情况下 先接收到包头
			{
				if (conn->buf_len >= HEAD_LEN) {
					int data_len = ntohl(*(int*)(data + 53));
					int pkt_len = data_len + HEAD_LEN;
					//	msg_t msg;
					PDUBase* pdu = new PDUBase;

					int len = conn->buf_len >= pkt_len ? pkt_len : conn->buf_len;
					_OnPduParse(data, len, *pdu);
					if (conn->buf_len >= pkt_len) {

						conn->buf_len -= pkt_len;
						conn->recv_pkt++;
						data += pkt_len;
						//	msg_info(msg);
						OnRecv(fd, pdu);
						
					}
					//not enough a pkt ;
					else {
						conn->pdu = pdu;//记录不完整pdu
						conn->pkt_len = pkt_len;
						conn->less_pkt_len = pkt_len - conn->buf_len;
						conn->buf_len = 0;
						break;
					}
				}
			}
			else {//找到包头
				LOGW("err data");
				int i;
				for (i = 0; i < conn->buf_len - 3; ++i) {
					if (ntohl(*((int*)(data + i))) == SPDUBase::startflag) {
						data += i;
						conn->buf_len -= i;
						break;
					}
				}
				if (i == conn->buf_len - 3) {
					conn->clear();
					break;
				}
			}
		}
		else {
			//处理不完整pdu
			int less_pkt_len = conn->less_pkt_len;
			PDUBase* pdu = conn->pdu;
			char* pData = pdu->body.get();
			int len = conn->buf_len >= less_pkt_len ? less_pkt_len : conn->buf_len;
			memcpy(pData + (conn->pkt_len - conn->less_pkt_len - HEAD_LEN), data, len);
			if (conn->buf_len >= less_pkt_len) {

				data += less_pkt_len;
				conn->buf_len -= less_pkt_len;
				//清空记录
				conn->pdu = NULL;
				conn->pkt_len = 0;
				conn->less_pkt_len = 0;
				conn->recv_pkt++;
				//msg_info(msg);
				OnRecv(fd, pdu);	
				
			}
			else {
				conn->less_pkt_len -= conn->buf_len;
				conn->buf_len = 0;
			}
		}
	}

	//move not enough header data to buf start
	if (conn->buf_len && data != conn->buf) {
		memmove(conn->buf, data, conn->buf_len);
	}
}

int ConnService::deleteClient(int _user_id)
{
}

void ConnService::Timer(int fd, short mask, void * privdata)
{
	ConnService* server = reinterpret_cast<ConnService*>(privdata);
	if (!server) {
		return;
	}
	server->reportOnliners();
	server->statistic();
}

void ConnService::reportOnliners()
{
	static uint64_t m_cronloops = 0;
	static uint64_t last_statistics_pkts = 0;
	
	Report_onliners req;
	req.set_onliners(m_conns);
	SPDUBase spdu;
	spdu.ResetPackBody(req, REPORT_ONLINERS);
	NodeMgr::getInstance()->sendSid(SID_LOADBALANCE, spdu);

//	printf("total recv %d pkts,every recv %d pkt,  total send:%d ,every sec send:%d \n",  total_recv_pkt, total_recv_pkt - sec_recv_pkt, total_send_pkts, total_send_pkts-sec_send_pkt);
	sec_recv_pkt = total_recv_pkt;
	sec_send_pkt = total_send_pkts;
}

void ConnService::count(int cmd)
{
	/*auto it = m_pkts.find(cmd);
	if (it == m_pkts.end()) {
		m_pkts[cmd] = 1;
	}
	else {
		++it->second;
	}*/
	++m_total_pkts;
	if (cmd == CID_CHAT_BUDDY || cmd == CID_CHAT_CUSTOMER_SERVICES || cmd == CID_CHAT_GROUP || cmd == CID_CHAT_MACHINE) {
		++m_total_chat_pkts;
	}
	
}

//statistic every 10 second
void ConnService::statistic()
{
	static long long loop = 0;
	static long long last_chat_msg_pkts = 0;
	static long long last_total_pkts = 0;
	if (loop & 1) {
		LOGI("recv msg rate -------chat msg:%d pkts/10sec----------- total msg:%d pkts/10sec", m_total_chat_pkts - last_chat_msg_pkts, m_total_pkts - last_total_pkts);
		last_chat_msg_pkts = m_total_chat_pkts;
		last_total_pkts = m_total_pkts;
	}
    ++loop;
}
