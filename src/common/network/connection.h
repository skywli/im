#ifndef _CONNECTION_H
#define _CONNECTION_H

#include "common/proto/pdu_base.h"
#include <im_queue.h>
#define RECV_BUF_LEN          16*1024


enum ConnState {
	CONN_STATE_AUTH,
	CONN_STATE_CONNECTED,
	CONN_STATE_ERROR,
	CONN_STATE_CLOSED,
	CONN_STATE_NONE
};

enum ConnType {
	CONN_TCP,
	CONN_UDP,
	CONN_HTTP,
	CONN_UNIX
};

#define  IO_ERROR           1
#define  IO_CLOSED          2
#define  IO_FINISH          3
#define  IO_AGAIN           4

typedef struct msg {
	char*m_data;
	int m_len;
	int m_alloc;
}msg_t;
bool  bufalloc(msg_t &msg, int len);
void  buffree(msg_t &msg);
class  Connection {
public:
	Connection();
	~Connection();
	bool empty();
	bool nextPkt();
	void clear();
	void push(msg_t& msg);

	int  write();
	int  read();

public:
	int                       fd;
								//recv pkt
	char                      buf[RECV_BUF_LEN];
	int                       buf_len;

	PDUBase*   			      pdu;
	int                       less_pkt_len;
	int                       pkt_len;

	//for send
	IMQueue<msg_t>            out_buffer;//store send data
	bool                      cache;

	msg_t                     out_pkt;   //store a pkt
	bool                      finish;   //a pkt whether send finish
	int                       send_len; //send pos

	void*                     pInstance;

	int                       recv_pkt;
	int                       send_pkt;

};

#endif

