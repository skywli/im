#include "transfer_client.h"
#include "log_util.h"

using namespace com::proto::basic;


TransferClient::TransferClient() {

}

void TransferClient::SetRegistInfo(std::string _ip, int _port)
{
	regist_ip_ = _ip;
	regist_port_ = _port;
}


void TransferClient::OnRecv(PDUBase* _base) {
	//connect_server.OnRoute(_base);
}

void TransferClient::OnConnect() {
	LOGD("service broadcast has established connection with transfer server");
//	RegistService(regist_ip_, regist_port_);
}

void TransferClient::OnDisconnect() {

}

void TransferClient::RegistService(std::string _ip, int _port)
{
	
}
