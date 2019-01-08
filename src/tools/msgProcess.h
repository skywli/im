#ifndef  _MSG_PROCESS_H
#define   _MSG_PROCESS_H
#include "threadBase.h"
#include <queue>
#include "lock.h"
class SPDUBase;
typedef void(*msgHandler)(int socket, SPDUBase* pdu);
typedef struct Args {
	int  socket;
	SPDUBase*  pdu;
}arg_t;
class MsgProcess:public ThreadBase
{
public:
	~MsgProcess();
	MsgProcess();
	int init(msgHandler handler, int num=1);

	//priority queue
	// priority: 0  insert back 1 insert front
	int addJob(int socket,SPDUBase* pdu,int priority=0);
	
	void start();
	void stop();
	void run();
private:

	mutex_t                   m_mutex;
	cond_t                    m_cond;
	std::deque<arg_t>         m_rbuf;
	bool                      m_stop;
	int                       m_num;
	msgHandler                m_handler;
};
#endif
