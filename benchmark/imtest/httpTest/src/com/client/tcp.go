package client

import (
	"net"
	"log"
	"strconv"
	"github.com/golang/protobuf/proto"
	"encoding/binary"
	"fmt"
	"bytes"
	"com/proto"
	"com/util"
)

type MsgContent struct {
	startFlag int32
	uid    int32
	cmd int32
	seq int32
	i int32
	byteSize int32
	body []byte
}
//建立连接
func start_client(i int,addr string) (net.Conn,error){
	//log.Println("线程"+strconv.Itoa(i)+":开始建立连接...")
	//log.Println("addr="+addr)
	if addr==""{
		addr=util.CFG.Section("service").Key("Addr").String()
	}
	conn, err := net.Dial("tcp",addr)
	if err != nil {
		log.Println("线程"+strconv.Itoa(i)+":连接建立失败:", err)
		return conn,err
	}

	log.Println("线程"+strconv.Itoa(i)+":连接建立成功 ok")
	return conn,nil
}
//发送组协议
func send(conn net.Conn,pb proto.Message,uid int32,cmd youmai.COMMANDID,msisdn string,i int,callback func(int,[]byte, string,int,chan string),reqchan chan string){
	ulpByte,_:=proto.Marshal(pb)
	//log.Print(ulpByte)
	var msg MsgContent
	msg.startFlag=123456789
	msg.uid=uid
	msg.cmd=int32(cmd)
	msg.seq=int32(i)
	msg.i=int32(0)
	msg.byteSize=int32(proto.Size(pb))
	msg.body=ulpByte


	sendbuf := new(bytes.Buffer)
	var binerr error
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.startFlag)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)
		return
	}
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.uid)
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
	binerr = binary.Write(sendbuf, binary.BigEndian, msg.i)
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
	conn.Write(sendbuf.Bytes())
	callbackfn(conn,msisdn,i,callback,reqchan,true)

}
//消息监听
func callbackfn(conn net.Conn,msisdn string,i int,callback func(int,[]byte, string,int,chan string),reqchan chan string,isout bool){
	buf := make([]byte,1024)
	for{

		lenght, err := conn.Read(buf)
		if err !=nil{
			log.Println("Server is dead ...",err)
			reqchan<-strconv.Itoa(4)
			conn.Close()
			return
		}

		cmd:=binary.BigEndian.Uint32(buf[8:12])
		//s:=binary.BigEndian.Uint32(buf[20:24])
		//log.Println(s)
		callback(int(cmd),buf[24:lenght],msisdn,i,reqchan)
		if isout{
			return
		}

	}
}
