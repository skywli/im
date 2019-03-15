
#include <string.h>
#include "msg_process.h"
#include <unistd.h>
#include <atomic>
#include <thread>
#include<cstdlib>
#include <log_util.h>
MsgProcess::~MsgProcess()
{
	destroy_cond(m_cond);
	destroy_mutex(m_mutex);
}

MsgProcess::MsgProcess() 
{
	m_num = 1;
	m_stop = false;
	m_handler = NULL;
	init_mutex(m_mutex);
	init_cond(m_cond);

}

int MsgProcess::addJob(int socket, SPDUBase* pdu, int priority)
{
	Args arg;
	arg.socket = socket;
	arg.pdu = pdu;
	lock_mutex(m_mutex);
	if (!priority) {
		m_rbuf.push_back(arg);
	}
	else {
		m_rbuf.push_front(arg);
	}
	
	signal_cond(m_cond);
	unlock_mutex(m_mutex);
	return 0;
}

int MsgProcess::init(msgHandler handler, int num)
{
	m_handler = handler;
	m_num = num;
}
void MsgProcess::stop()
{
	m_stop = true;
}
void MsgProcess::start()
{
	start_thread(m_num);
}

void MsgProcess::run()
{
	while (!m_stop)
	{
		lock_mutex(m_mutex);
		while (m_rbuf.empty()) {
			wait_cond(m_cond, m_mutex);
		}
		arg_t  arg;
		arg = m_rbuf.front();
		m_rbuf.pop_front();
		unlock_mutex(m_mutex);
		if (m_handler) {
			m_handler(arg.socket, arg.pdu);
		}
		else {
			LOGE("not set msg handler");
			return;
		}
		
	}
	stop_thread();

}
