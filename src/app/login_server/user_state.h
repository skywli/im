#pragma once
#include <time.h>
#include <string>

struct User {
	User(const std::string&  user_id);
	int clear();
	
	std::string device_id_;
	std::string userid_;

	int nid_;
	int sockfd_;

	int  device_type_;

	int online_status_;
	int          version;
	time_t       online_time; //online time;
};