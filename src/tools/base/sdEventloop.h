#ifndef ___SDEVENTLOOP_H__
#define ___SDEVENTLOOP_H__
typedef  int sd_socket_t;
typedef void sdFileProc(sd_socket_t fd, short mask, void *clientData);
typedef void  sdTimeProc(sd_socket_t fd, short mask, void *clientData);
typedef void sdBeforeSleepProc();

#define SD_NONE     0
#define SD_READABLE 1
#define SD_WRITABLE 2

#define SD_FILE_EVENTS 1
#define SD_TIME_EVENTS 2
#define SD_ALL_EVENTS (SD_FILE_EVENTS|SD_TIME_EVENTS)
#define SD_DONT_WAIT 4

#define SD_NOMORE -1
#define SD_DELETED_EVENT_ID -1

/* Macros */
#define SD_NOTUSED(V) ((void) V)

#define    MIN_FD_SIZE                    256

class SdEventLoop {
public :
	SdEventLoop() {}
	virtual ~SdEventLoop() {  }
	virtual int init(int size) = 0;
	virtual int createFileEvent(sd_socket_t fd, short mask, sdFileProc *proc, void *clientData) = 0;
	virtual int deleteFileEvent(sd_socket_t fd, short mask) = 0;
	virtual long long  createTimeEvent(long long milliseconds, sdTimeProc *proc, void *clientData) = 0;
	virtual int deleteTimeEvent(long long id) = 0;
	virtual void main() = 0;
	virtual void stop() = 0;

};

 SdEventLoop* getEventLoop();
#endif


