#ifndef _NETUTIL_H
#define _NETUTIL_H

int createTcpServer(const char* ip, short port);
int createTcpClient(const char * ip, short port);
int acceptTcpConnection(int fd);
int setNonBlock(int fd);
#endif
