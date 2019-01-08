#ifndef _TIME_UTIL_H
#define _TIME_UTIL_H

#include <string>
#ifdef WIN32
#include <Windows.h>
typedef     DWORD  TIME_T;

#else
#include <sys/time.h>
typedef      struct timeval    TIME_T;
#endif

std::string timestamp_datetime();
long long timestamp_int();
long long get_mstime();

void getCurTime(TIME_T* time);

/**
* ¾«È·µ½ºÁÃë
*/
int   interval(TIME_T start, TIME_T stop);
#endif


