#ifndef _TYPEDEF_H
#define _TYPEDEF_H
#include<log_util.h>
#include <string>
typedef int Socketfd_t;
typedef std::string UserId_t;
typedef int socket_t;

#define CHECK_PB_PARSE_MSG(ret, msg) {            \
    if (ret == false)                            \
    {                                            \
        LOGE("%s: parse pb msg failed.",msg);   \
        return false;                            \
    }                                            \
}
#endif
