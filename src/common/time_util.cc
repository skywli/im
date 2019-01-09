#include "time_util.h"


std::string timestamp_datetime() {
	std::string datetime;
	char datatimetmp[25] = "";
	time_t t;
	tm *tmp;

	t = time(nullptr);
	tmp = localtime(&t);
	strftime(datatimetmp, 24, "%Y-%m-%d %H:%M:%S", tmp);
	datetime = datatimetmp;

	return datetime;
}

long long timestamp_int() {
	return get_mstime();
}

long long get_mstime() {
	struct timeval tv;
	long long mst;

	gettimeofday(&tv, NULL);
	mst = ((long long)tv.tv_sec) * 1000;
	mst += tv.tv_usec / 1000;
	return mst;
}

void getCurTime(TIME_T* time)
{
#ifdef WIN32
	*time = GetTickCount();
#else
	gettimeofday(time, NULL);
#endif
}

int interval(TIME_T start, TIME_T stop)
{
#ifdef WIN32
	return stop - start;
#else
	if (stop.tv_usec < start.tv_usec) {
		stop.tv_usec += 1000 * 1000;
		stop.tv_sec -= 1;
	}
	return (stop.tv_sec - start.tv_sec) * 1000 + (stop.tv_usec - start.tv_usec) / 1000;
#endif
	return 0;
}
