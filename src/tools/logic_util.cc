#include "logic_util.h"

namespace LogicUtil {

    std::string build_session_id(int _src_user_id, int _type, int _dst_user_id) {
        return (std::to_string(_src_user_id) + "_" + std::to_string(_type) + "_" + std::to_string(_dst_user_id));
    }

    std::string get_phone(std::string _str) {
        return _str.substr(_str.find(":") + 1);
    }
}
