#ifndef _TCP_CLIENT_H
#define _TCP_CLIENT_H

#include "pdu_base.h"
#include "pdu_util.h"
#include <string>
#include <mutex>
#include <google/protobuf/message.h>

class TcpClient: public PduUtil {
public:
    TcpClient();
    ~TcpClient();

    int Send(SPDUBase &_base);
    int SendProto(google::protobuf::Message &_msg, int _command_id, int _seq_id=0);

    int Connect(const char *_ip, int _port, bool _is_reconnect = true);
    int Close();
    void Run();

    virtual void OnRecv(PDUBase* _base) = 0;
    virtual void OnConnect() = 0;
    virtual void OnDisconnect() = 0;

private:
    const int resend_num_limit_ = 1; // 发送失败重发次数限制
    int sockfd_;
    bool is_threadrun_;
    std::recursive_mutex sockfd_mutex_;

protected:
    int port_;
    std::string ip_;
    bool is_reconnect_;
};

#endif
