#include "pdu_util.h"
#include "log_util.h"
#include <arpa/inet.h>
#include <string.h>
#include <deleter.h>
PduUtil::PduUtil() {

}

/*
 * PDU解析
 */
int PduUtil::OnPduParse(const char *_buf, int _length, PDUBase &_base/*this is return value*/) {
	return _base.OnPduParse(_buf, _length);
}
int PduUtil::_OnPduParse(const char *_buf, int _length, PDUBase &_base/*this is return value*/) {
    return _base._OnPduParse(_buf, _length);
}

/*
 * PDU封包
 */
int PduUtil::OnPduPack(PDUBase &_base, std::shared_ptr<char> &_outbuf/*this is return value*/) {
	return _base.OnPduPack(_outbuf);
}

/*
* PDU封包
*/
int PduUtil::OnPduPack(PDUBase &_base, char*& _outbuf/*this is return value*/) {
	return _base.OnPduPack(_outbuf);
}
