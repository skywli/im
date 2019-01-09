#include "pdu_base.h"
#include <string.h>
PDUBase::PDUBase() {
    memset(terminal_token ,0,sizeof(terminal_token));
    this->length = 0;
    this->seq_id = 0;
    this->command_id = 0;
    this->version = 0;
}
