#ifndef _LOG_UTIL_H
#define _LOG_UTIL_H

enum LogLevel {
    Level_Debug = 1,
    Level_Trace = 2,
    Level_Info = 3,
    Level_Warning = 4,
    Level_Error = 5,
    Level_Fatal = 6,
} ;

void LogImpl(LogLevel l, const char *file, int line,  const char *msg, ...);
void LogImpl(LogLevel l, const char *file, int line, const char *func, int command_id, int seq_id, const char *msg, ...);
int initLog(const char* filename,int level);
#define LOGD(x, ...) LogImpl(Level_Debug, __FILE__, __LINE__, x, ## __VA_ARGS__)
#define LOGT(x, ...) LogImpl(Level_Trace, __FILE__, __LINE__,  x, ## __VA_ARGS__)
#define LOGI(x, ...) LogImpl(Level_Info, __FILE__, __LINE__,  x, ## __VA_ARGS__)
#define LOGW(x, ...) LogImpl(Level_Warning, __FILE__, __LINE__,  x, ## __VA_ARGS__)
#define LOGE(x, ...) LogImpl(Level_Error, __FILE__, __LINE__,  x, ## __VA_ARGS__)

#define LOGDEBUG(command_id, seq_id, x, ...) LogImpl(Level_Debug, __FILE__, __LINE__, __FUNCTION__, command_id, seq_id, x, ## __VA_ARGS__)
#define LOGTRACE(command_id, seq_id, x, ...) LogImpl(Level_Trace, __FILE__, __LINE__, __FUNCTION__, command_id, seq_id, x, ## __VA_ARGS__)
#define LOGINFO(command_id, seq_id, x, ...) LogImpl(Level_Info, __FILE__, __LINE__, __FUNCTION__, command_id, seq_id, x, ## __VA_ARGS__)
#define LOGWARN(command_id, seq_id, x, ...) LogImpl(Level_Warning, __FILE__, __LINE__, __FUNCTION__, command_id, seq_id, x, ## __VA_ARGS__)
#define LOGERROR(command_id, seq_id, x, ...) LogImpl(Level_Error, __FILE__, __LINE__, __FUNCTION__, command_id, seq_id, x, ## __VA_ARGS__)

#endif
