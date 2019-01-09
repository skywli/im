#include <event.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <event2/event-config.h>
#include <stdint.h>
#include <event2/buffer.h>
#include "http_server.h"
#include <cstdlib>
#include <thread>
#include <log_util.h>
#include <mutex>
#include <netutil.h>

HttpServer::HttpServer()
{
	m_size = 1;
    m_timeid=0;
}

HttpServer::~HttpServer()
{
}

int HttpServer::setThreads(int size)
{
	m_size = size;
	m_base = new struct event_base* [m_size];
	m_threads = new std::thread*[m_size];
	m_httpds = new struct evhttp *[m_size];
	for (int i = 0; i < m_size; i++) {
		m_base[i] = event_base_new();
		if (m_base[i] == NULL) {
	        LOGE("event base new fail");
			return -1;
		}
		m_httpds[i] = evhttp_new(m_base[i]);
		if (m_httpds == NULL) {
			LOGE("evhttp init fail");
			return -1;
		}
        evhttp_set_timeout(m_httpds[i],2);

		evhttp_set_gencb(m_httpds[i], request_cb, this);
	}
	return 0;
}

int HttpServer::init(const std::string & ip, short port,int size)
{
	m_ip = ip;
	m_port = port;
	m_size = size;
	if (-1 == setThreads(m_size)) {
		return -1;
	}
	m_fd = createTcpServer(ip.c_str(), port);
	if (m_fd == -1) {
		return -1;
	}
    return 0;
}

int HttpServer::start()
{
	for (int i = 0; i < m_size; i++) {
		if (-1 == evhttp_accept_socket(m_httpds[i], m_fd)) {
			LOGE("evhttp_accept_socket fail");
			return -1;
		}
		m_threads[i] = new std::thread(thread_proc,m_base[i]);
        m_threads[i]->detach();
	}
	return 0;
}

long long HttpServer::createTimeEvent(long long milliseconds, sdTimeProc * proc, void * clientData)
{
	struct event *timeout = new struct event;
	struct timeval tv;
	event_assign(timeout, m_base[0], -1, 0, proc, clientData);
	evutil_timerclear(&tv);
	tv.tv_sec = milliseconds / 1000;
	tv.tv_usec = (milliseconds % 1000) * 1000;
	event_add(timeout, &tv);

	++m_timeid;
	//m_timeevents[m_timeid] = timeout;
	return m_timeid;
}

int HttpServer::thread_proc(struct event_base* base)
{
	event_base_dispatch(base);
}

/*struct addrinf{
    const std::string ip;
    short       port;
    int         num;
}addrs[2]={{"192.168.0.42",8003,0},{"192.168.0.42",8004,0}};

std::recursive_mutex mutex;
int getaddr(std::string& ip,short& port){
    std::lock_guard<std::recursive_mutex> lock_1(mutex);
    printf("num1:%d,num2:%d\n",addrs[0].num,addrs[1].num);
    if(addrs[0].num<addrs[1].num){
        ip=addrs[0].ip;
        port=addrs[0].port;
        addrs[0].num++;
    }
    else{
        ip=addrs[1].ip;
        port=addrs[1].port;
        addrs[1].num++;
    }


}*/
void HttpServer::request_cb(evhttp_request * req, void * arg)
{
	HttpServer* s = static_cast<HttpServer*>(arg);
	if (s == NULL) {
		return;
	}
	const char *cmdtype;
	struct evkeyvalq *headers;
	struct evkeyval *header;
	struct evbuffer *buf;

	switch (evhttp_request_get_command(req)) {
	case EVHTTP_REQ_GET: cmdtype = "GET"; break;
	case EVHTTP_REQ_POST: cmdtype = "POST"; break;
	case EVHTTP_REQ_HEAD: cmdtype = "HEAD"; break;
	case EVHTTP_REQ_PUT: cmdtype = "PUT"; break;
	case EVHTTP_REQ_DELETE: cmdtype = "DELETE"; break;
	case EVHTTP_REQ_OPTIONS: cmdtype = "OPTIONS"; break;
	case EVHTTP_REQ_TRACE: cmdtype = "TRACE"; break;
	case EVHTTP_REQ_CONNECT: cmdtype = "CONNECT"; break;
	case EVHTTP_REQ_PATCH: cmdtype = "PATCH"; break;
	default: cmdtype = "unknown"; break;
	}

	s->request_handler(req);

}
