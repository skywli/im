#pragma once
#include <string>
#include <define.h>
class Statistics {
public:
	Statistics();
	~Statistics();
	int init(std::string& fileName);
	int addRecvMsg(uint64_t msg_id, const std::string& sender, const std::string& recver, uint64_t recv_time,int type);
	int addAckMsg(uint64_t msg_id, uint64_t ack_time,uint64_t latency_time);
private:
	int append(std::string& data);
private:
	struct tm    m_logsTime;
	std::string  m_fileName;
	int          m_fd;

};