#include <sd_event_loop.h>
#include <lb_event_loop.h>

SdEventLoop * getEventLoop()
{
	return new LbEventLoop;
}
