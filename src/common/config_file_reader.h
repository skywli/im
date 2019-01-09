/*
 * 2017.04.07
 * lngwu11@qq.com
 */

#ifndef _CONFIG_FILE_READER_H
#define _CONFIG_FILE_READER_H

#include <string>
#include <tinyxml.h>
#include <IM.Basic.pb.h>
using namespace com::proto::basic;
// public.conf
#define CONF_LOG          "../conf/log.conf"
#define CONF_PUBLIC_URL   "../public.conf"
#define CONF_MONITOR_URL   "../conf/monitor.xml"
#define CONF_REDIS_IP     "redis_ip"
#define CONF_REDIS_PORT   "redis_port"
#define CONF_REDIS_AUTH   "redis_auth"

#define CONF_MSG_QUEUE_IP  "msg_queue_ip"
#define CONF_MSG_QUEUE_PORT "msg_queue_port"
#define CONF_MSG_QUEUE_AUTH   "msg_queue_auth"

#define CONF_REDIS_EXPIRE "redis_expire"
#define CONF_DBPROXY_IP   "dbproxy_ip"
#define CONF_DBPROXY_PORT  "dbproxy_port"
#define CONF_ROUTE_IP     "route_ip"
#define CONF_ROUTE_PORT   "route_port"
#define CONF_MONITOR_IP     "monitor_ip"
#define CONF_MONITOR_PORT   "monitor_port"


#define CONF_MGQUEUE_IP     "mq_ip"
#define CONF_MGQUEUE_PORT   "mq_port"
//loadbalance

#define CONF_LOADBALANCE_HTTP_IP     "loadbalance_http_ip"
#define CONF_LOADBALANCE_HTTP_PORT   "loadbalance__http_port"

//broadcast

#define CONF_BROADCAST_HTTP_IP     "broadcast_http_ip"
#define CONF_BROADCAST_HTTP_PORT   "broadcast_http_port"


#define CONF_TRANSFER_IP     "transfer_ip"
#define CONF_TRANSFER_PORT   "transfer_port"

// conn
#define CONF_CONN_URL      "../conf/conn.xml"
#define CONF_CONN_OUT_IP       "out_ip"
#define CONF_CONN_OUT_PORT     "out_port"
#define CONF_CONN_IN_IP       "conn_in_ip"
#define CONF_CONN_IN_PORT     "conn_in_port"


#define  CONF_LISTEN_IP      "ip"
#define  CONF_LISTEN_PORT    "port"
#define  CONF_SERVICE_TYPE   "service_type"
#define  CONF_SERVICEES      "services"
#define  CONF_CONN_TYPE      "conn_type"
#define  CONF_NODE_ID        "node_id"

//dispatch
#define CONF_DISPATCH_URL      "../conf/dispatch.xml"

//login
#define CONF_LOGIN_URL      "../conf/login.xml"

//msg
#define CONF_MSG_URL      "../conf/msg.xml"
//buddy
#define CONF_BUDDY_URL      "../conf/buddy.xml"
//group
#define CONF_GROUP_URL      "../conf/group.xml"
//loadbalance
#define CONF_LOADBALANCE_URL      "../conf/loadbalance.xml"
//broadcast
#define CONF_BROADCAST_URL      "../conf/broadcast.xml"

// state broadcast

#define CONF_STATE_BROADCAST_IP      "state_ip"
#define CONF_STATE_BROADCAST_PORT    "state_port"
#define CONF_STATE_BROADCAST_AUTH    "state_auth"
typedef struct {
	int sid;
	const char* name;
}SidInfo;
const char* sidName(int sid);
int getSid(const char* name);
typedef std::multimap<std::string, std::string>::iterator  MIT;
class ConfigFileReader {
	
public:
 
	static ConfigFileReader* getInstance();
    ~ConfigFileReader();

	int init(const char * file);

	int ReadInt(const char * key);

	std::string ReadString(const char * key);

	void setConfigValue(const char * key, const char * value);
	TiXmlElement* getRoot();
	
	bool getNodePointerByName(TiXmlElement * pRootEle, const char * strNodeName, TiXmlElement *& destNode);
	int save();
private:
    ConfigFileReader();
private:
	TiXmlDocument doc;
	TiXmlElement* root;
	const char*  filename;
};


#endif
