
#ifndef _TRANSFER_CLIENT_H
#define _TRANSFER_CLIENT_H

#include "tcp_client.h"
#include <IM.Basic.pb.h>
#include "typedef.h"

using namespace com::proto::basic;

class TransferClient :public TcpClient {
public:
	TransferClient();

	void SetRegistInfo(std::string _ip, int _port);

	virtual void OnRecv(PDUBase* _base);
	virtual void OnConnect();
	virtual void OnDisconnect();
private:
	std::string regist_ip_;
	int regist_port_;

	void RegistService(std::string _ip, int _port);
};

#endif

