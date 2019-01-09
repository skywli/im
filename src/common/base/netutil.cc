#include "netutil.h"
#include <sys/stat.h>
#include <sys/socket.h>
#include <cstring>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
int createTcpServer(const char * ip, short port)
{
	int r;
	int nfd;
	nfd = socket(AF_INET, SOCK_STREAM, 0);
	if (nfd < 0) return -1;
	int one = 1;
	r = setsockopt(nfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);

	r = bind(nfd, (struct sockaddr*)&addr, sizeof(addr));
	if (r < 0) return -1;
	r = listen(nfd, 1024);
	if (r < 0) return -1;

	int flags;
	setNonBlock(nfd);

	return nfd;
}

int setNonBlock(int fd) {
	int flags;
	if ((flags = fcntl(fd, F_GETFL, 0)) < 0
		|| fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return -1;
}
int acceptTcpConnection(int fd) {
	struct sockaddr_storage ss;
#ifdef WIN32
	int slen = sizeof(ss);
#else
	socklen_t slen = sizeof(ss);
#endif
	int cfd = ::accept(fd, (struct sockaddr*)&ss, &slen);
	if (cfd <= 0)
	{
		return -1;
	}
	return cfd;
}

void getsockInfo(int fd, short* port, char* ip,int ip_len) {
	socklen_t len;
	struct sockaddr_storage sa;
	len = sizeof(sa);
	getsockname(fd, (struct sockaddr *)&sa, &len);
	if (sa.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&sa;
		if (ip) inet_ntop(AF_INET, (void*)&(s->sin_addr), ip, ip_len);
		if (port) *port = ntohs(s->sin_port);
	}
	else {
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
		if (ip) inet_ntop(AF_INET6, (void*)&(s->sin6_addr), ip, ip_len);
		if (port) *port = ntohs(s->sin6_port);
	}
}


