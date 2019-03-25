#include <event.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <event2/event-config.h>
#include <stdint.h>
#include <signal.h>

#ifdef WIN32
#pragma warning(disable: 4996)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#endif
#include <vector>
#include<cstring>

#include "lb_event_loop.h"
#define DEFAULT_MAX_CONN  1024

LbEventLoop::LbEventLoop()
{
    m_size = DEFAULT_MAX_CONN+ MIN_FD_SIZE;
    m_timeid = 0;
	evthread_use_pthreads();
	m_base = event_base_new();
	m_events = (lbFileEvent*)malloc(sizeof(lbFileEvent)*m_size);
	for (int i = 0; i<m_size; i++) {
		m_events[i].mask = SD_NONE;
	}
}

LbEventLoop::~LbEventLoop()
{
    if(m_base){
        event_base_free(m_base);
    }
    if(m_events){
        free(m_events);
    }
}

int LbEventLoop::init(int size)
{
	m_size = size + MIN_FD_SIZE;
	if (m_events) {
		free(m_events);
	}
	m_events = (lbFileEvent*)malloc(sizeof(lbFileEvent)*m_size);
	for (int i = 0; i<m_size; i++) {
		m_events[i].mask = SD_NONE;
	}
	return 0;
  
}

int LbEventLoop::createFileEvent(sd_socket_t fd, short mask, sdFileProc * proc, void * clientData)
{
    if (fd >= m_size) {
        return -1;
    }
    std::lock_guard<std::recursive_mutex> lock_1(m_mutex);
    lbFileEvent *fe = &m_events[fd];
    if(fe->mask & mask){
        return -1;
    }
    fe->mask |= mask;

    struct event* pEv;
    if (mask & SD_READABLE) {
        pEv = &fe->revent;
        memset(pEv, 0, sizeof(struct event));
        event_set(pEv, fd, EV_READ | EV_PERSIST, proc, clientData);
    }
    else if (mask & SD_WRITABLE) {
        pEv = &fe->wevent;
        memset(pEv, 0, sizeof(struct event));
        event_set(pEv, fd, EV_WRITE | EV_PERSIST, proc, clientData);
    }

    event_base_set(m_base, pEv);
    event_add(pEv, NULL);
    return 0;
}

int LbEventLoop::deleteFileEvent(sd_socket_t fd, short mask)
{
    std::lock_guard<std::recursive_mutex> lock_1(m_mutex);
    if (fd >= m_size) return -1;
    lbFileEvent *fe = &m_events[fd];
    if (fe->mask == SD_NONE) return -1;

    if (fe->mask & mask & SD_READABLE) {
        event_del(&fe->revent);
    }
    if (fe->mask & mask & SD_WRITABLE) {
        event_del(&fe->wevent);
    }
    fe->mask = fe->mask & (~mask);
}

long long LbEventLoop::createTimeEvent(long long milliseconds, sdTimeProc * proc, void * clientData)
{
    struct event *timeout = new struct event;
    struct timeval tv;
    event_assign(timeout, m_base, -1, EV_PERSIST, proc, clientData);
    evutil_timerclear(&tv);
    tv.tv_sec = milliseconds/1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    event_add(timeout, &tv);
    std::lock_guard<std::recursive_mutex> lock_1(m_mutex);
    ++m_timeid;
    m_timeevents[m_timeid] = timeout;
    return m_timeid;
}

int LbEventLoop::deleteTimeEvent(long long id)
{
    std::lock_guard<std::recursive_mutex> lock_1(m_mutex);
    auto it = m_timeevents.find(id);
    if (it != m_timeevents.end()) {
        event_del(it->second);
        delete it->second;
        m_timeevents.erase(it);
    }
    return 0;
}

void LbEventLoop::main()
{
    m_stop = 0;
    while (!m_stop) {
        event_base_dispatch(m_base);
    }
}

void LbEventLoop::stop()
{
    m_stop = 1;
    event_base_loopbreak(m_base);
}
