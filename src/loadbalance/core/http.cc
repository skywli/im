#include "http.h"
#include <log_util.h>
#include <loadBalance.h>

extern LoadBalanceServer loadbalance_server;
int num=0;
Http::Http()
{
}

Http::~Http()
{
}

int Http::request_handler(evhttp_request * req)
{
	const char* uri = evhttp_request_get_uri(req);
	//	logd("Received a %s request for %s\nHeaders:", cmdtype, uri);
	char* decode_uri = evhttp_decode_uri(uri);
	LOGD("decode uri:%s", decode_uri);
	struct evkeyvalq params;
	evhttp_parse_query(decode_uri, &params);

	const char* userid = evhttp_find_header(&params, "userid");
	if (userid == NULL) {
		LOGE("not find");
		return -1;
	}

	std::string ip;
	short port;
	loadbalance_server.AllocateLoadbalance(ip, port, userid);
	struct evbuffer* evb = evbuffer_new();
	evbuffer_add_printf(evb, "{ \"ip\": \"%s\",\"port\":%d }", ip.c_str(), port);
	evhttp_send_reply(req, 200, "OK", evb);
    num++;
    LOGE("total request:%d",num);
    evbuffer_free(evb);
}
