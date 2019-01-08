#include "user_state.h"
#include <IM.Basic.pb.h>

using namespace com::proto::basic;

int User::clear()
{
	device_type_ = DeviceType_UNKNOWN;
	online_status_ = USER_STATE_LOGOUT;
	version = 0;
	online_time = 0;
	nid_ = 0;
	sockfd_ = 0;
	device_id_.clear();
}
