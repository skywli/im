#pragma once
#include "instance.h"

void Instance::parse(Connection* conn) {
	int fd = conn->fd;
	char* data = conn->buf;
	while (conn->buf_len >= SHEAD_LEN) {
		//没有不完整的包
		if (!conn->less_pkt_len) {
			short* startflag = (short*)(data);
			if (ntohs(*(startflag)) == SPDUBase::serverflag)//正常情况下 先接收到包头
			{
				if (conn->buf_len >= SHEAD_LEN) {
					int data_len = ntohl(*(reinterpret_cast<int*>(data + 63)));
					int pkt_len = data_len + SHEAD_LEN;
					//	msg_t msg;
					SPDUBase* pdu = new SPDUBase;

					int len = conn->buf_len >= pkt_len ? pkt_len : conn->buf_len;
					pdu->_OnPduParse(data, len);
					if (conn->buf_len >= pkt_len) {

						conn->buf_len -= pkt_len;
						conn->recv_pkt++;
						data += pkt_len;
						//	msg_info(msg);
						onData(fd, pdu);
					}
					//not enough a pkt ;
					else {
						conn->pdu = pdu;//记录不完整pdu
						conn->pkt_len = pkt_len;
						conn->less_pkt_len = pkt_len - conn->buf_len;
						conn->buf_len = 0;
						break;
					}
				}
			}
			else {//找到包头
				LOGW("err data");
				int i;
				for (i = 0; i < conn->buf_len - 1; ++i) {
					if (ntohl(*((int*)(data + i))) == SPDUBase::serverflag) {
						data += i;
						conn->buf_len -= i;
						break;
					}
				}
				if (i == conn->buf_len - 1) {
					conn->clear();
					break;
				}
			}
		}
		else {
			//处理不完整pdu
			int less_pkt_len = conn->less_pkt_len;
			SPDUBase* pdu = dynamic_cast<SPDUBase*>(conn->pdu);
			char* pData = pdu->body.get();
			int len = conn->buf_len >= less_pkt_len ? less_pkt_len : conn->buf_len;
			memcpy(pData + (conn->pkt_len - conn->less_pkt_len - SHEAD_LEN), data, len);
			if (conn->buf_len >= less_pkt_len) {

				data += less_pkt_len;
				conn->buf_len -= less_pkt_len;

				//清空记录
				conn->pdu = NULL;
				conn->pkt_len = 0;
				conn->less_pkt_len = 0;
				conn->recv_pkt++;
				//msg_info(msg);
				onData(fd, pdu);
			}
			else {
				conn->less_pkt_len -= conn->buf_len;
				conn->buf_len = 0;
			}
		}
	}

	//move not enough header data to buf start
	if (conn->buf_len && data != conn->buf) {
		memmove(conn->buf, data, conn->buf_len);
	}
}


