#pragma once
#include "common/proto/pdu_base.h"
enum  ConnectionEvent {
	Disconnected,
	Connected,
};

class Instance {
public:
	virtual void onData(int fd, PDUBase* _base) = 0;
	virtual void onEvent(int fd, ConnectionEvent event) = 0;
};

