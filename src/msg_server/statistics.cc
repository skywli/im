#include "statistics.h"
#include <json/json.h>
#include <unistd.h>
#include <log_util.h>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
Statistics::Statistics()
{
	m_fd = -1;
}

Statistics::~Statistics()
{
}

int Statistics::init(std::string & fileName)
{
	m_fileName = fileName;
	m_fd = open(m_fileName.c_str(), O_RDWR | O_CREAT| O_APPEND, 0666);
	if (m_fd == -1) {
		LOGE("open file:%s fail(%s)", fileName.c_str(), strerror(errno));
		return -1;
	}

	struct stat statBuf;
	int res;
	time_t t;

	// obtain last modification time
	res = stat(fileName.c_str(), &statBuf);
	if (res < 0) {
		t = time(NULL);
	}
	else {
		t = statBuf.st_mtime;
	}

	localtime_r(&t, &m_logsTime);
	return 0;
}

int Statistics::addRecvMsg(uint64_t msg_id, const std::string & sender, const std::string & recver, uint64_t recv_time, int type)
{
	if (m_fd == -1) {
		return 0;
	}
	Json::FastWriter  fwriter;
	Json::Value root;
	root["id"] = msg_id;
	root["sender"] = sender;
	root["recver"] = recver;
	root["time"] = recv_time;
	root["imType"] = type;
	root["t"] = "r";
	std::string object = fwriter.write(root);
	append(object);
	return 0;
}

int Statistics::addAckMsg(uint64_t msg_id, uint64_t ack_time, uint64_t latency_time)
{
	if (m_fd == -1) {
		return 0;
	}
	Json::FastWriter  fwriter;
	Json::Value root;
	root["id"] = msg_id;
	root["ack"] = ack_time;
	root["latency"] = latency_time;
	root["t"] = "a";
	std::string object = fwriter.write(root);
	append(object);
	return 0;
}

int Statistics::append(std::string & data)
{

	struct tm now;
	time_t t = time(NULL);

	bool timeok = localtime_r(&t, &now) != NULL;

	if (timeok) {
		if ((now.tm_mday != m_logsTime.tm_mday) ||
			(now.tm_mon != m_logsTime.tm_mon) ||
			(now.tm_year != m_logsTime.tm_year)) {

			close(m_fd);
			std::ostringstream filename_s;

			filename_s << m_fileName << "." << m_logsTime.tm_year + 1900 << "-"
				<< std::setfill('0') << std::setw(2) << m_logsTime.tm_mon + 1 << "-"
				<< std::setw(2) << m_logsTime.tm_mday << std::ends;
			const std::string lastFn = filename_s.str();
			rename(m_fileName.c_str(), lastFn.c_str());

			m_fd = open(m_fileName.c_str(), O_RDWR | O_CREAT| O_APPEND, 0666);
			if (m_fd == -1) {
				LOGE("open file:%s fail(%s)", m_fileName.c_str(), strerror(errno));
				return -1;
			}
			m_logsTime = now;
		}
	}

	if (write(m_fd, data.data(), data.length()) == -1) {
		LOGE("write file:%s fail", m_fileName.c_str());
		return -1;
	}
	return 0;


}
