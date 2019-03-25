#ifndef _LOGIC_UTIL_H
#define _LOGIC_UTIL_H

#include <string>

namespace LogicUtil {
    std::string build_session_id(int _src_user_id, int _type, int _dst_user_id);
    std::string get_phone(std::string _str);
}

#endif
