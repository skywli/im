#include "sdEventloop.h"
#include <lbEventLoop.h>

SdEventLoop * getEventLoop()
{
	return new LbEventLoop;
}
