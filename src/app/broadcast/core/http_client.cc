#include <event.h>
#include <event2/util.h>
#include <event2/http.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
    static void
http_request_done(struct evhttp_request *req, void *ctx)
{
    char buffer[256];
    int nread;

    if (req == NULL) {
        /* If req is NULL, it means an error occurred, but
         * sadly we are mostly left guessing what the error
         * might have been.  We'll do our best... */
        struct bufferevent *bev = (struct bufferevent *) ctx;
        unsigned long oslerr;
        int printed_err = 0;
        int errcode = EVUTIL_SOCKET_ERROR();
        fprintf(stderr, "some request failed - no idea which one though!\n");
        /* If the OpenSSL error queue was empty, maybe it was a
         * socket error; let's try printing that. */
        if (! printed_err)
            fprintf(stderr, "socket error = %s (%d)\n",
                    evutil_socket_error_to_string(errcode),
                    errcode);
        return;
    }

    fprintf(stderr, "Response line: %d %s\n",
            evhttp_request_get_response_code(req),
            evhttp_request_get_response_code_line(req));

    while ((nread = evbuffer_remove(evhttp_request_get_input_buffer(req),
                    buffer, sizeof(buffer)))
            > 0) {
        /* These are just arbitrary chunks of 256 bytes.
         * They are not lines, so we can't treat them as such. */
        fwrite(buffer, nread, 1, stdout);
    }
}

int httpClient(const char* url){
    int r;
    char uri[256];
    struct evhttp_uri *http_uri = NULL;
    struct bufferevent *bev;
    struct evhttp_connection *evcon = NULL;
    struct evkeyvalq *output_headers;
    const char *scheme,*host,*path, *query;
    struct evhttp_request *req;
    int ret = 0;
    int port;
    struct event_base *base;
    base = event_base_new();
    if (!base) {
        perror("event_base_new()");
        goto error;
    }
    http_uri = evhttp_uri_parse(url);
    if (http_uri == NULL) {
        goto error;
    }

    host = evhttp_uri_get_host(http_uri);
    if (host == NULL) {
        goto error;
    }

    port = evhttp_uri_get_port(http_uri);
    if (port == -1) {
        port =  80 ;
    }

    path = evhttp_uri_get_path(http_uri);
    if (strlen(path) == 0) {
        path = "/";
    }

    query = evhttp_uri_get_query(http_uri);
    if (query == NULL) {
        snprintf(uri, sizeof(uri) - 1, "%s", path);
    } else {
        snprintf(uri, sizeof(uri) - 1, "%s?%s", path, query);
    }
    uri[sizeof(uri) - 1] = '\0';
    printf("uri:%s\n",uri);
    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    if (bev == NULL) {
        fprintf(stderr, "bufferevent_openssl_socket_new() failed\n");
        goto error;
    }
    evcon = evhttp_connection_base_bufferevent_new(base, NULL, bev,
            host, port);
    if (evcon == NULL) {
        fprintf(stderr, "evhttp_connection_base_bufferevent_new() failed\n");
        goto error;
    }

    evhttp_connection_set_retries(evcon, 2);	
    evhttp_connection_set_timeout(evcon, 2);
    // Fire off the request
    req = evhttp_request_new(http_request_done, bev);
    if (req == NULL) {
        fprintf(stderr, "evhttp_request_new() failed\n");
        goto error;
    }

    output_headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(output_headers, "Host", host);
    evhttp_add_header(output_headers, "Connection", "close");
    r = evhttp_make_request(evcon, req,  EVHTTP_REQ_GET, uri);
    if (r != 0) {
        fprintf(stderr, "evhttp_make_request() failed\n");
        goto error;
    }

    event_base_dispatch(base);
    goto cleanup;
error:
    ret = 1;

cleanup:
    if (evcon)
        evhttp_connection_free(evcon);
    if (http_uri)
        evhttp_uri_free(http_uri);
    event_base_free(base);

    return ret;
}

