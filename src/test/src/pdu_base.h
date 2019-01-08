/******************
 * author@xiesong
 * songtzu@126.com
 * 4.12.2016
 */
#ifndef _PDU_BASE_H
#define _PDU_BASE_H

#include <memory> // shared_ptr

#define HEAD_LEN   57
class PDUBase {
public:
    PDUBase();

	/********************************************
	* index 0, [0,4)
	* start flag.
	*/
	const static int startflag = 0x66aa;

	/********************************************
	* index 1, [4,40)
	* terminal_id is user_id, if user loged in.
	* if not login, given  him a random int.
	*
	*/
	char terminal_token[36];

	/*********************************************
	* index 3, [40,44)
	* this stand for service id.
	* meet with protobuf.
	*/
	int service_id;

	/*********************************************
	* index 4, [44,48)
	* this stand for command id.
	* meet with protobuf.
	*/
	int command_id;

	/*********************************************
	* index 5, [48,52)
	* seq_id, app (also in other connect in model,
	* such like php, used to route back to real source.)
	* in app, seq_id is used to dispatch the event to different source.
	*/
	int seq_id;

	/*********************************************
	* index 6, [52,53)
	* pdu verions define.
	*/
	char version;

	/*********************************************
	* index 7, [53,57)
	* protobuf length. in binary format.
	*/
	int length;

	/*********************************************
	* the buffer holder for protobuf.
	* this use shared_ptr to manage memory.
	*/
	std::shared_ptr<char> body;
};

#endif
