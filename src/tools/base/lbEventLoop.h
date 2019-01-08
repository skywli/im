#ifndef __RD_EVENTLOOP_H
#define __RD_EVENTLOOP_H

#include "sdEventloop.h"
#include <mutex>
#include <map>
#include <event.h>
/* File event structure */
typedef struct lbFileEvent {
	int mask; /* one of AE_(READABLE|WRITABLE) */
	struct event revent;
	struct event wevent;
} lbFileEvent;

class LbEventLoop :public SdEventLoop {
public:
	LbEventLoop();
	~LbEventLoop();
	int init(int size);
	int createFileEvent(sd_socket_t fd, short mask, sdFileProc *proc, void *clientData);
	int deleteFileEvent(sd_socket_t fd, short mask);
	long long  createTimeEvent(long long milliseconds, sdTimeProc *proc, void *clientData);
	int deleteTimeEvent(long long id);
	void main();
	void stop();
private:
	struct event_base* m_base;
	lbFileEvent*       m_events;
	int                m_size;
    std::recursive_mutex m_mutex;
	int                m_stop;
	int                m_timeid;
	std::map<int, struct event*>    m_timeevents;
};

#endif


