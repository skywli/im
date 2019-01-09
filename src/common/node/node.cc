#include "node.h"
#include <cstring>
#include <IM.Server.pb.h>
using namespace com::proto::server;
Node::Node(uint16_t sid,uint32_t nid)
{
	m_sid = sid;
	m_id = nid;
	memset(m_ip, 0, sizeof(m_ip));
	m_state = NODE_UNABLE;
	type = NODE_SERVER;

	regist_time = 0;
	auth_time = 0;
	ping_time = 0;
	pong_time = 0;
	m_fd = -1;
}

void Node::clear()
{
	m_state = NODE_UNABLE;
	regist_time = 0;
	auth_time = 0;
	ping_time = 0;
	pong_time = 0;
	m_fd = -1;
}

uint32_t Node::getNodeId()
{
	return m_id;
}

void Node::setsock(int fd)
{
	m_fd = fd;
}

int Node::getsock()
{
	return m_fd;
}

int Node::getState()
{
	return m_state;
}
uint16_t Node::getSid()
{
	return m_sid;
}
void Node::setNid(uint32_t nid)
{
	m_id=nid;
}
uint32_t Node::getNid()
{
	return m_id;
}
void Node::setState(int state)
{
	m_state = state;
}

void Node::setType(uint16_t type)
{
	m_sid = type;
}

uint16_t Node::getType()
{
	return m_sid;
}

int Node::setAddrs(const char * ip, uint16_t port)
{
	memset(m_ip, 0, sizeof(m_ip));
	m_port = port;
	strcpy(m_ip, ip);
	return 0;
}

const char * Node::getIp()
{
	return m_ip;
}

uint16_t Node::getPort()
{
	return m_port;
}



