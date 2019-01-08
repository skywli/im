#include "pdu_base.h"
#include <arpa/inet.h>
#include <log_util.h>

PDUBase::PDUBase()
	:service_id(0),
	command_id(0),
	seq_id(0),
	version(0),
	length(0){
    memset(terminal_token,0,36);
}
PDUBase::~PDUBase()
{
}
PDUBase::PDUBase(const PDUBase& base){
    service_id=base.service_id;
    command_id=base.command_id;
    seq_id=base.seq_id;
    version=base.version;
    length=base.length;
    memcpy(terminal_token,base.terminal_token,sizeof(terminal_token));
    body=base.body;
}
 PDUBase& PDUBase:: operator = (const PDUBase& base){
    
    service_id=base.service_id;
    command_id=base.command_id;
    seq_id=base.seq_id;
    version=base.version;
    length=base.length;
    memcpy(terminal_token,base.terminal_token,sizeof(terminal_token));
    body=base.body;
    return *this;
}
int PDUBase::OnPduParse(const char *_buf, int _length/*this is return value*/) {
	if (_length < HEAD_LEN) return 0;
    const char *buf = _buf;
	//client header
	const int *flag = reinterpret_cast<const int*>(buf);
	if (ntohl(*flag) != startflag) {
		LOGE("ntohs(*startflag) != SPDUBase::startflag, %d", ntohl(*flag));
		return -1;
	}
	buf += sizeof(startflag);

	memcpy(terminal_token,buf,sizeof(terminal_token));
	buf += sizeof(terminal_token);

	service_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(service_id);

	command_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(command_id);

	seq_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(seq_id);

	version = *buf;
	buf += sizeof(version);

	length = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(length);


	//LOGD("before print length %d", buf - _buf);
	//LOGD("数据全部长度%d", this->length);

	if (buf - _buf + this->length > _length) {
		//LOGE("not a full pack, %d + %d > %d", buf - _buf, this->length, _length);
		return 0;
	}
	std::shared_ptr<char> pbody(new char[this->length], carray_deleter);
	memcpy(pbody.get(), buf, this->length);
	this->body = pbody;
	return buf - _buf + this->length;
}

int PDUBase::_OnPduParse(const char *_buf, int _length/*this is return value*/) {
	if (_length < HEAD_LEN) return 0;
	const char *buf = _buf;
	//client header
	const int *flag = reinterpret_cast<const int*>(buf);
	if (ntohl(*flag) != startflag) {
		LOGE("ntohs(*startflag) != SPDUBase::startflag, %d", ntohl(*flag));
		return -1;
	}
	buf += sizeof(startflag);

	memcpy(terminal_token, buf, sizeof(terminal_token));
	buf += sizeof(terminal_token);

	service_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(service_id);

	command_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(command_id);

	seq_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(seq_id);

	version = *buf;
	buf += sizeof(version);

	length = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(length);


	std::shared_ptr<char> pbody(new char[this->length], carray_deleter);
	memcpy(pbody.get(), buf, _length - (buf - _buf));
	this->body = pbody;
	return 0;
	//   return buf - _buf + this->length;
}
/*
* PDU封包
*/
int PDUBase::OnPduPack(std::shared_ptr<char> &_outbuf/*this is return value*/) {
	
	int total_len = CHEAD_LEN + length;
	std::shared_ptr<char> sp_buf(new char[total_len], carray_deleter);
	char *buf = sp_buf.get();

	*(reinterpret_cast<int*>(buf))= htonl(startflag);
	buf += sizeof(startflag);

	memcpy(buf, terminal_token, sizeof(terminal_token));
	buf += sizeof(terminal_token);

	*(reinterpret_cast<int*>(buf)) = htonl(service_id);
	buf += sizeof(service_id);

	*(reinterpret_cast<int*>(buf)) = htonl(command_id);
	buf += sizeof(command_id);

	*(reinterpret_cast<int*>(buf)) = htonl(seq_id);
	buf += sizeof(seq_id);

	*buf = version;
	buf += sizeof(version);

	*(reinterpret_cast<int*>(buf)) = htonl(length);
	buf += sizeof(length);

	memcpy(buf, this->body.get(), length);

	_outbuf = sp_buf;
	return total_len;
}

/*
* PDU封包
*/
int PDUBase::OnPduPack(char*& _outbuf/*this is return value*/) {
	int total_len = CHEAD_LEN + length;

	char *sp_buf = new char[total_len];

    char* buf=sp_buf;
	*(reinterpret_cast<int*>(buf)) = htonl(startflag);
	buf += sizeof(startflag);

	memcpy(buf, terminal_token, sizeof(terminal_token));
	buf += sizeof(terminal_token);

	*(reinterpret_cast<int*>(buf)) = htonl(service_id);
	buf += sizeof(service_id);

	*(reinterpret_cast<int*>(buf)) = htonl(command_id);
	buf += sizeof(command_id);

	*(reinterpret_cast<int*>(buf)) = htonl(seq_id);
	buf += sizeof(seq_id);

	*buf = version;
	buf += sizeof(version);

	*(reinterpret_cast<int*>(buf)) = htonl(length);
	buf += sizeof(length);

	memcpy(buf, this->body.get(), length);

	_outbuf = sp_buf;
	return total_len;
}


void PDUBase::ResetPackBody(google::protobuf::Message &_msg, int _commandid) {
	std::shared_ptr<char> _body(new char[_msg.ByteSize()], carray_deleter);
	_msg.SerializeToArray(_body.get(), _msg.ByteSize());
	body = _body;
	length = _msg.ByteSize();
	command_id = _commandid;
}

void carray_deleter(char * ptr)
{
	if (ptr) {
		delete[] ptr;
	}
}

SPDUBase::~SPDUBase()
{
}

SPDUBase::SPDUBase()
{
    node_id=0;
    sockfd=0;
    
}

SPDUBase::SPDUBase(const PDUBase & base):PDUBase(base)
{
    node_id=0;
    sockfd=0;
}
SPDUBase::SPDUBase(const SPDUBase & base):PDUBase(base)
{
    node_id=base.node_id;
    sockfd=base.sockfd;
}
SPDUBase& SPDUBase:: operator = (const SPDUBase& base){
    
    node_id=base.node_id;
    sockfd=base.sockfd;
    service_id=base.service_id;
    command_id=base.command_id;
    seq_id=base.seq_id;
    version=base.version;
    length=base.length;
    memcpy(terminal_token,base.terminal_token,sizeof(terminal_token));
    body=base.body;
    return *this;
}

int SPDUBase::OnPduParse(const char *_buf, int _length/*this is return value*/) {
	if (_length < SHEAD_LEN) return 0;

	const char *buf = _buf;

	//server header
	const short *sflag = reinterpret_cast<const short*>(buf);
	if (ntohs(*sflag) != serverflag) {
		LOGE("ntohs(*serverflag) != SPDUBase::serverflag, %d", ntohs(*sflag));
		return -1;
	}
	buf += sizeof(serverflag);

	node_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(node_id);

	sockfd = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(sockfd);


	//client header
	const int *flag = reinterpret_cast<const int*>(buf);
	if (ntohl(*flag) != startflag) {
		LOGE("ntohs(*startflag) != SPDUBase::startflag, %d", ntohl(*flag));
		return -1;
	}
	buf += sizeof(startflag);

	memcpy(terminal_token, buf, sizeof(terminal_token));
	buf += sizeof(terminal_token);

	service_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(service_id);

	command_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(command_id);

	seq_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(seq_id);

	version = *buf;
	buf += sizeof(version);

	length = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(length);


	//LOGD("before print length %d", buf - _buf);
	//LOGD("数据全部长度%d", this->length);

	if (buf - _buf + this->length > _length) {
		//LOGE("not a full pack, %d + %d > %d", buf - _buf, this->length, _length);
		return 0;
	}
	std::shared_ptr<char> pbody(new char[length], carray_deleter);
	memcpy(pbody.get(), buf,length);
	this->body = pbody;
	return buf - _buf + length;
}

int SPDUBase::_OnPduParse(const char *_buf, int _length/*this is return value*/) {
	if (_length < SHEAD_LEN) return 0;

	const char *buf = _buf;

	//server header
	const short *sflag = reinterpret_cast<const short*>(buf);
	if (ntohs(*sflag) != serverflag) {
		LOGE("ntohs(*serverflag) != SPDUBase::serverflag, %d", ntohs(*sflag));
		return -1;
	}
	buf += sizeof(serverflag);

	node_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(node_id);

	sockfd = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(sockfd);


	//client header
	const int *flag = reinterpret_cast<const int*>(buf);
	if (ntohl(*flag) != startflag) {
		LOGE("ntohs(*startflag) != SPDUBase::startflag, %d", ntohl(*flag));
		return -1;
	}
	buf += sizeof(startflag);

	memcpy(terminal_token, buf, sizeof(terminal_token));
	buf += sizeof(terminal_token);

	service_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(service_id);

	command_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(command_id);

	seq_id = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(seq_id);

	version = *buf;
	buf += sizeof(version);

	length = ntohl(*(reinterpret_cast<const int*>(buf)));
	buf += sizeof(length);


	std::shared_ptr<char> pbody(new char[length], carray_deleter);
	memcpy(pbody.get(), buf, _length - (buf - _buf));
	body = pbody;
	return 0;
	//   return buf - _buf + this->length;
}
/*
* PDU封包
*/
int SPDUBase::OnPduPack(std::shared_ptr<char> &_outbuf/*this is return value*/) {
	
	int total_len = SHEAD_LEN + length;
	std::shared_ptr<char> sp_buf(new char[total_len], carray_deleter);
	char *buf = sp_buf.get();
	//server header
	*(reinterpret_cast<short*>(buf)) = htons(serverflag);

	buf += sizeof(serverflag);
	
	*(reinterpret_cast<int*>(buf)) = htonl(node_id);
	buf += sizeof(node_id);

	*(reinterpret_cast<int*>(buf)) = htonl(sockfd);
	buf += sizeof(sockfd);

	//client header
	*(reinterpret_cast<int*>(buf)) = htonl(startflag);
	buf += sizeof(startflag);

	memcpy(buf, terminal_token, sizeof(terminal_token));
	buf += sizeof(terminal_token);

	*(reinterpret_cast<int*>(buf)) = htonl(service_id);
	buf += sizeof(service_id);

	*(reinterpret_cast<int*>(buf)) = htonl(command_id);
	buf += sizeof(command_id);

	*(reinterpret_cast<int*>(buf)) = htonl(seq_id);
	buf += sizeof(seq_id);

	*buf = version;
	buf += sizeof(version);

	*(reinterpret_cast<int*>(buf)) = htonl(length);
	buf += sizeof(length);

	memcpy(buf, this->body.get(), length);

	_outbuf = sp_buf;
	return total_len;
}
/*
* PDU封包
*/
int SPDUBase::OnPduPack(char*& _outbuf/*this is return value*/) {

	int total_len = SHEAD_LEN + length;
	char * sp_buf = new char[total_len];
    char* buf=sp_buf;
	
	//server header
	*(reinterpret_cast<short*>(buf)) = htons(serverflag);
	buf += sizeof(serverflag);

	*(reinterpret_cast<int*>(buf)) = htonl(node_id);
	buf += sizeof(node_id);

	*(reinterpret_cast<int*>(buf)) = htonl(sockfd);
	buf += sizeof(sockfd);

	//client header
	*(reinterpret_cast<int*>(buf)) = htonl(startflag);
	buf += sizeof(startflag);

	memcpy(buf, terminal_token, sizeof(terminal_token));
	buf += sizeof(terminal_token);

	*(reinterpret_cast<int*>(buf)) = htonl(service_id);
	buf += sizeof(service_id);

	*(reinterpret_cast<int*>(buf)) = htonl(command_id);
	buf += sizeof(command_id);

	*(reinterpret_cast<int*>(buf)) = htonl(seq_id);
	buf += sizeof(seq_id);

	*buf = version;
	buf += sizeof(version);

	*(reinterpret_cast<int*>(buf)) = htonl(length);
	buf += sizeof(length);

	memcpy(buf, this->body.get(), length);

	_outbuf = sp_buf;
	return total_len;
}
