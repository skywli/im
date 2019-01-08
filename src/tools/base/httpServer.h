#ifndef _HTTPSERVER_H
#define _HTTPSERVER_H
#include <event.h>
#include <thread>
#include <string>
#include <event2/event.h>
#include <event2/http.h>
typedef void  sdTimeProc(int fd, short mask, void *clientData);
class HttpServer {
public:
	HttpServer();
	~HttpServer();
	virtual int request_handler(struct evhttp_request *req) = 0;
	int init(const std::string& ip, short port,int size=1);
	int start();

	long long createTimeEvent(long long milliseconds, sdTimeProc * proc, void * clientData);
	
	static int   thread_proc(struct event_base* base);
	static void request_cb(struct evhttp_request *req, void *arg);
private:
	int setThreads(int size);
private:
	struct event_base**         m_base;
	struct evhttp **            m_httpds;
	std::thread**               m_threads;
	int                         m_size;
	short                       m_port;
	std::string                 m_ip;
	int                         m_fd;
    int                         m_timeid;
};

#endif
