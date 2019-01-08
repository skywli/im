#ifndef _NODE_H
#define _NODE_H

#include <time.h>
#include <stdint.h>
#include<list>
#include <string>
#define NODE_CLIENT     1
#define NODE_SERVER     2
#define CLUSTER_SLOTS                 256

class Node {
public:
	Node(uint16_t sid, uint32_t nid);
	uint32_t getNodeId();
	void setsock(int fd);
	int getsock();
	int getState();
	uint16_t getSid();
    void setNid(uint32_t nid);
	uint32_t getNid();
	void setState(int state);
	void setType(uint16_t type);
	uint16_t getType();
	int  setAddrs(const char* ip, uint16_t port);
	const char* getIp();
	uint16_t getPort();
	
	void clear();
private:
	uint32_t   m_id;
	char       m_ip[16];
	uint16_t   m_port;
	int        m_state;
	uint16_t   m_sid;  //service type
	int        m_fd;

public:
	int          type;
	time_t       regist_time;
	time_t       auth_time;
	time_t       pong_time;
	time_t       ping_time;

	//route
	int stragry;
	std::list<std::string> user_list;
	std::string slot_str;
	char slots[CLUSTER_SLOTS / 8];

};
#endif
