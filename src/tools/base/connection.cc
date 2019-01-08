#include "connection.h"

#include <cstring>
#include <unistd.h>


extern  uint32_t total_send_pkt;
extern uint32_t send_fail_pkts;
Connection::Connection()
{
	memset(buf, 0, sizeof(buf));
	pdu = NULL;

	buf_len = 0;
	less_pkt_len = 0;
	pkt_len = 0;
	
	//out_buffer = new IMQueue<msg_t>;
	send_len = 0;
	
	finish = true;

	cache = true;
	pInstance = NULL;
	recv_pkt = 0;
	send_pkt = 0;
}

Connection::~Connection()
{
	if (pdu) {
		delete pdu;
	}
	buf_len = 0;
	less_pkt_len = 0;

}

bool Connection::empty()
{
	return finish && out_buffer.empty();
}

bool Connection::nextPkt()
{
	if (!finish) {
		//msg = conn->out_pkt;
		return true;
	}
	else if (!out_buffer.empty()) {
		out_buffer.pop(out_pkt);
		//		msg = conn->out_pkt;
		finish = false;
		return true;
	}
	return false;
}

void Connection::clear()
{
    printf("clear buf\n");
	memset(buf, 0, sizeof(buf));
	if (pdu) {
		delete pdu;
	}
	buf_len = 0;
	less_pkt_len = 0;
	pkt_len = 0;
}

void Connection::push(msg_t & msg)
{
	out_buffer.push(msg);
}

int Connection::write()
{
	size_t nwritten = 0;
	size_t data_len;

	while (nextPkt()) {
		msg_t& data = out_pkt;
		data_len = data.m_len;
		nwritten = ::write(fd, data.m_data + send_len, data_len - send_len);
		if (nwritten == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return IO_AGAIN;
			}
			else {
				//++send_fail_pkts;
				return IO_ERROR;
			}
		}
		else if (nwritten == 0) {
			return IO_CLOSED;
		}
		send_len += nwritten;

		if (send_len == data_len) {
			//printf("send fd(%d)  %d bytes\n", fd,  send_len);
			//++total_send_pkt;
			send_len = 0;
			finish = true;
			buffree(out_pkt);
		}
	}
	return IO_FINISH;
}

int Connection::read()
{
	int recv_len = RECV_BUF_LEN - buf_len;
	int nRcv = 0;
	int res = 0;
	while (recv_len) {
		nRcv = ::read(fd, buf + buf_len, recv_len);//每次先取包头长度 
		if (nRcv < 0)
		{
			if (errno == EINTR) {
				continue;
			}
			else if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return IO_AGAIN;
			}
			else {
				return IO_ERROR;
			}
		}
		else if (nRcv == 0)
		{
			return IO_CLOSED;
		}
		else {
			buf_len += nRcv;
			recv_len -= nRcv;
		}
	}

	return IO_FINISH;
}

bool bufalloc(msg_t & msg, int len)
{
	msg.m_data = NULL;
	msg.m_len = 0;
	msg.m_alloc = 0;
	msg.m_data = new char[len];
	if (msg.m_data)
	{
		msg.m_alloc = len;
		return true;
	}
	return false;
}

void  buffree(msg_t& msg)
{
	if (msg.m_data != NULL)
	{
		delete[] msg.m_data;
		msg.m_data = NULL;
	}
}
