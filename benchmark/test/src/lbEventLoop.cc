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

#include "lbEventLoop.h"


LbEventLoop::LbEventLoop()
{
	m_size = MIN_FD_SIZE;
}

LbEventLoop::~LbEventLoop()
{
}

int LbEventLoop::init(int size)
{
	m_size = size + MIN_FD_SIZE;
#ifdef WIN32
	WSADATA		wsaData;
	DWORD		Ret;
	if ((Ret = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		printf("WSAStartBug\n");
	}
	evthread_use_windows_threads();
#else
	evthread_use_pthreads();
#endif
	m_base = event_base_new();
	m_events = (lbFileEvent*)malloc(sizeof(lbFileEvent)*m_size);
    for(int i=0;i<m_size;i++){
        m_events[i].mask=SD_NONE;
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
	return 0;
}

int LbEventLoop::deleteTimeEvent(long long id)
{
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
