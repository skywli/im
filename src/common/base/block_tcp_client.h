#ifndef _BLOCK_TCP_CLIENT_H
#define _BLOCK_TCP_CLIENT_H

#include <string>
#include <mutex>
#include "pdu_util.h"
#include <google/protobuf/message.h>

class BlockTcpClient:public PduUtil {
public:
    BlockTcpClient();
    ~BlockTcpClient();

    int Connect(const char *_ip, int _port, bool _is_reconnect=false);
    int Send(PDUBase &_base);
    int Read(PDUBase &_base);
    int SendProto(google::protobuf::Message &_msg, int _command_id, int _seq_id=0);

private:
    std::string ip_;
    int port_;
    int sockfd_;

    bool is_reconnect_;
    std::recursive_mutex send_mutex_;

    char *total_buffer_;
    char *buf_;
};

#endif
