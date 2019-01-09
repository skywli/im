package client

import (
	"bytes"
	"com/proto"
	"com/util"
	"encoding/binary"
	"fmt"
	"github.com/golang/protobuf/proto"
	"log"
	"net"
	"strconv"
)

type MsgContent struct {
	startFlag int32
	uid       string
	sid       int32
	cmd       int32
	seq       int32
	version   int8
	byteSize  int32
	body      []byte
}

//建立连接
func start_client(i int, addr string) (net.Conn, error) {
	//log.Println("线程"+strconv.Itoa(i)+":开始建立连接...")
	//log.Println("addr="+addr)
	if addr == "" {
		addr = util.CFG.Section("service").Key("Addr").String()
	}
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		log.Println("线程"+strconv.Itoa(i)+":连接建立失败:", err)
		return conn, err
	}

	log.Println("线程" + strconv.Itoa(i) + ":连接建立成功 ok")
	return conn, nil
}

//发送组协议
func send(conn net.Conn, pb proto.Message, user_id string, cmd youmai.COMMANDID, i int, callback func(int, []byte, string, int, chan string), reqchan chan string) {
	ulpByte, _ := proto.Marshal(pb)
	//log.Print(ulpByte)
	var msg MsgContent
	msg.startFlag = int32(0x66aa)
	msg.uid = user_id
	msg.sid = int32(0)
	msg.cmd = int32(cmd)
	msg.seq = int32(i)
	msg.version = int8(0)
	msg.byteSize = int32(proto.Size(pb))
	msg.body = ulpByte

	sendbuf := new(bytes.Buffer)
	var binerr error
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.startFlag)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}
	binerr = binary.Write(sendbuf, binary.LittleEndian, []byte(msg.uid))
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.sid)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.cmd)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.seq)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.version)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.byteSize)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}
	binerr = binary.Write(sendbuf, binary.LittleEndian, msg.body)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}

	//log.Println(ulpByte)
	//log.Println(proto.Size(pb))
	//log.Println(sendbuf.Len())
	//fmt.Println("body size:", msg.byteSize, "buf size:", sendbuf.Len())
	conn.Write(sendbuf.Bytes())
	callbackfn(conn, user_id, i, callback, reqchan, true)

}

//消息监听
func callbackfn(conn net.Conn, user_id string, i int, callback func(int, []byte, string, int, chan string), reqchan chan string, isout bool) {
	//声明一个临时缓冲区，用来存储被截断的数据
	tmpBuffer := make([]byte, 0)
	buf := make([]byte, 1024*17) //packet len must less than 8k,
	for {

		buflen, err := conn.Read(buf)
		//log.Println("read:", buflen)
		if err != nil {
			log.Println("Server is dead ...", err)
			reqchan <- strconv.Itoa(4)
			conn.Close()
			return
		}

		cmd := binary.BigEndian.Uint32(buf[44:48])
		protoSize := binary.BigEndian.Uint32(buf[53:57])
		//size:=len(buf)
		//log.Println(protoSize)
		tmpBuffer = append(tmpBuffer, buf[:buflen]...)
		//log.Println(len(tmpBuffer))
		if len(tmpBuffer) >= int(protoSize)+57 {
			callback(int(cmd), tmpBuffer[57:int(protoSize)+57], user_id, i, reqchan)
			tmpBuffer = tmpBuffer[int(protoSize)+57:]
		} else {
			log.Println("not enough")
		}
		//s:=binary.BigEndian.Uint32(buf[20:24])
		//log.Println(s)
		if isout {
			return
		}

	}
}
