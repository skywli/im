#ifndef _PDU_UTIL_H
#define _PDU_UTIL_H

#include "base/pdu_base.h"

class PduUtil {
public:
    PduUtil();

    /***********************************************************
     * @PDU解析
     * if, default pdu did not meet your requirement, override this function.
     * if, error format, return -1.
     * if, has package, return readed package.
     * if, does not have a full package, return 0.
     * if, IT'S OVERRIDED, OnPduPack also need to be override.
     */
    virtual int OnPduParse(const char *_buf, int _length, PDUBase &_base);
     int _OnPduParse(const char *_buf, int _length, PDUBase &_base);


    /***********************************************************
     * @PDU封包
     */
    virtual int OnPduPack(PDUBase &_base, std::shared_ptr<char> &_outbuf);
	int OnPduPack(PDUBase & _base, char *& _outbuf);
};

#endif
