#include<log4cpp/Category.hh>
#include<log4cpp/OstreamAppender.hh>
#include<log4cpp/Priority.hh>
#include<log4cpp/PatternLayout.hh>
#include<log4cpp/FileAppender.hh>
#include<log4cpp/Category.hh>
#include<log4cpp/PropertyConfigurator.hh>
#include "log_util.h"
#include <stdarg.h>
#include <string>
#include <cstring>
#include <fstream>
#define IM_ERR -1
#define IM_OK   0
#define MAX_DEBUG_LEN            2048
static log4cpp::Category* s_log = NULL;
static int s_level = Level_Error;
int vs_log_stub(LogLevel l, const char *file, int line, const char *func, int command_id, int seq_id, const char *content, va_list ap) {
	if (l < s_level) {
		return -1;
	}
    const char *LogLevelString[] = {"DEBUG", "TRACE", "INFO", "WARNING", "ERROR", "FATAL"};

   const char* pfile = strrchr(file, '/');
	if (NULL == pfile) {
		return -1;
	}

    char data[MAX_DEBUG_LEN] = {0};
    int bytes = snprintf(data, MAX_DEBUG_LEN-1, "[command_id:%d] [seq_id:%d] [FILE:%s:%d] [FUNC:%s] ",
                       command_id, seq_id, pfile, line, func);

    bytes += vsnprintf(data + bytes, MAX_DEBUG_LEN - 1 - bytes, content, ap);
      
	
    std::string msg(data);
    switch (l)
	{
	case Level_Fatal:
		s_log->fatal(msg);
		break;
	/*case LOG_CRIT:
		s_log->crit(msg);
		break;*/
	case Level_Error:
		s_log->error(msg);
		break;
	case Level_Warning:
		s_log->warn(msg);
		break;
	case Level_Debug:
		s_log->debug(msg);
		break;
	default:
		s_log->debug(msg);
		break;
	}
}

int initLog(const char* filename,int level)
{
    s_level=level;
	if (level > Level_Fatal) {
		s_level = Level_Fatal;
	}
	if (level < Level_Debug) {
		s_level = Level_Debug;
	}
	try
	{
		log4cpp::PropertyConfigurator::configure(filename);//导入配置文件
		log4cpp::Category& log = log4cpp::Category::getInstance(std::string("error"));
		s_log = &log;
		if (!s_log)
		{
			fprintf(stderr, "Configure error\n");
			return IM_ERR;
		}
	}
	catch (log4cpp::ConfigureFailure& f)
	{
		fprintf(stderr, "Configure Problem: %s\n", f.what());
		return IM_ERR;
	}
	return IM_OK;
}

void  LogImpl(LogLevel level, const char* filename, int line, const char* format, ...)
{
	if (level < s_level) {
		return;
	}
	if (!s_log) {
		return;
	}
	char buf[MAX_DEBUG_LEN + 1] = { 0 };
	int num = 0;
#ifdef WIN32
	const char* p = strrchr(filename, '\\');
	if (NULL == p) {
		return;
	}
	num = sprintf(buf, "%s:%d ", p + 1, line);
#else
	const char* p = strrchr(filename, '/');
	if (NULL == p) {
		return;
	}
	num = sprintf(buf, "%s:%d ", p + 1, line);
#endif
	va_list vl;
	va_start(vl, format);
	vsnprintf(buf + num, MAX_DEBUG_LEN - num, format, vl);
	va_end(vl);
	//printf("%s\n", buf);
	std::string msg(buf);
	switch (level)
	{
	case Level_Fatal:
		s_log->fatal(msg);
		break;
	/*case LOG_CRIT:
		s_log->crit(msg);
		break;*/
	case Level_Error:
		s_log->error(msg);
		break;
	case Level_Warning:
		s_log->warn(msg);
		break;
	case Level_Info:
		s_log->info(msg);
		break;
	case Level_Debug:
		s_log->debug(msg);
		break;
	default:
		s_log->debug(msg);
		break;
	}
}

void LogImpl(LogLevel l, const char * file, int line, const char * func, int command_id, int seq_id, const char * msg, ...)
{
  va_list ap;
    int ret_val = 0;

    va_start(ap,msg);
    ret_val = vs_log_stub(l, file, line, func, command_id, seq_id, msg, ap);
    va_end(ap);
}

