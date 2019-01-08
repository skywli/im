#ifndef _HTTP_H_
#define _HTTP_H_
#include <httpServer.h>
class Http :public HttpServer {
public:
	Http();
	~Http();
	int request_handler(struct evhttp_request *req);

};
#endif
