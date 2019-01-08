package client

import (
	"bytes"
	"com/proto"
	"com/util"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"github.com/golang/protobuf/proto"
	"io/ioutil"
	"log"
	"net"
	"net/http"
	"strconv"
	"time"
)

type CommetMap struct {
	user_id string
	err     error
	conn    net.Conn
}

//建立连接池
func ConnectionPoolInit(users []string) map[string]net.Conn {
	cp := make(map[string]net.Conn)
	statime := time.Now().UnixNano() / 1e6
	poolchan := make(chan CommetMap)
	i := 1
	sleep := util.CFG.Section("def").Key("connSleep").String()
	s, _ := time.ParseDuration(sleep)
	//log.Println(usermap)
	for t := range users {
		//fmt.Println(t+"="+strconv.Itoa(i))

		time.Sleep(s)
		go Connection(poolchan, i, users[t])
		i++
	}

	reqok := 0
	reqerr := 0

	extcount := len(users)
	for x := range poolchan {
		//log.Println(x+":"+strconv.Itoa(count))
		//log.Printf("xx0")
		if x.err != nil {
			reqerr++
		}
		if x.conn != nil {
			cp[x.user_id] = x.conn
			reqok++
		}

		extcount--
		if extcount <= 0 { // 如果现有数据量为0，跳出循环
			break
		}

	}

	endtime := time.Now().UnixNano() / 1e6
	andtime := endtime - statime
	log.Printf("连接建立完成！总数：%d|成功创建连接数：%d|失败创建连接数:%d\n", len(users), reqok, reqerr)
	log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime, float64(andtime)/float64(len(users)))
	return cp
}

type AddMap struct {
	Ip   string
	Port int
}

//起线程连接
func Connection(poolchan chan CommetMap, i int, user_id string) {
	var cm = CommetMap{}
	cm.user_id = user_id
	addr := ""
	if util.CFG.Section("service").Key("HttpUrl").String() != "" {
		url := ""
		url = util.CFG.Section("service").Key("HttpUrl").String() + "/api/v1/sum?userid=" + user_id
		resp, err := http.Get(url)
		if err != nil {
			if err != nil {
				cm.err = err
				poolchan <- cm
				log.Println("线程"+strconv.Itoa(i)+":连接负载均衡失败:", err)
				return
			}
		}

		defer resp.Body.Close()
		body, err := ioutil.ReadAll(resp.Body)
		if err != nil {
			if err != nil {
				cm.err = err
				poolchan <- cm
				log.Println("线程"+strconv.Itoa(i)+":获取负载均衡失败:", err)
				return
			}
		}

		addmsp := AddMap{}
		bodystr := string(body)
		//log.Println(bodystr)
		jsonerr := json.Unmarshal([]byte(bodystr), &addmsp)
		if jsonerr != nil {
			cm.err = jsonerr
			poolchan <- cm
			log.Println("线程"+strconv.Itoa(i)+":解析json失败:", jsonerr)
			return
		}

		//log.Println(addmsp)
		addr = fmt.Sprintf("%s:%d", addmsp.Ip, addmsp.Port)

	}
	//start connect server
	conn, err := start_client(i, addr)
	if err != nil {
		cm.err = err
		poolchan <- cm
		return
	}
	cm.conn = conn
	poolchan <- cm
	//defer conn.Close()
}

func LoginStart(conns map[string]net.Conn) {
	statime := time.Now().UnixNano() / 1e6
	loginchan := make(chan string)
	sleep := util.CFG.Section("def").Key("loginSleep").String()
	s, _ := time.ParseDuration(sleep)
	i := 1
	for user_id := range conns {
		time.Sleep(s)
		go login(user_id, conns[user_id], i, loginchan)
		i++
	}
	loginok := 0
	loginerr := 0
	loginconnerr := 0
	extcount := len(conns)
	for x := range loginchan {
		//log.Println(x+":"+strconv.Itoa(count))
		if x == "1" {
			loginok++
			//log.Println("login success num:", loginok)
		}
		if x == "2" {
			loginerr++
		}
		if x == "4" {
			loginconnerr++
		}
		extcount--
		if extcount <= 0 { // 如果现有数据量为0，跳出循环
			break
		}

	}
	endtime := time.Now().UnixNano() / 1e6
	andtime := endtime - statime
	log.Printf("登录完成！总数：%d|成功登录数：%d|失败登录数:%d|登录连接失败数:%d\n", len(conns), loginok, loginerr, loginconnerr)
	log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime, float64(andtime)/float64(len(conns)))
	return
}
func login(user_id string, conn net.Conn, i int, loginchan chan string) {
	ulp := &youmai.User_Login{
		UserId: proto.String(user_id),
		AppId:  proto.String("ICEYOUMAI-631E-4ED8-968D-F0A6F82DBCA7"),

		DeviceId: proto.String("123456789"),
	}

	send(conn, ulp, user_id, youmai.COMMANDID_USER_LOGIN, i, tcpCallback, loginchan)
}

//监听登录回调
func loing_cb(rproto []byte, user_id string, i int, reqchan chan string) {
	loingack := &youmai.User_Login_Ack{}
	proto.Unmarshal(rproto, loingack)
	if loingack.GetErrerNo() == youmai.ERRNO_CODE_ERRNO_CODE_OK {
		log.Print("线程" + strconv.Itoa(i) + ":" + user_id + "登录成功")
		reqchan <- strconv.Itoa(1)
	} else {
		log.Print("线程" + strconv.Itoa(i) + ":" + user_id + "登录失败" + loingack.GetErrerNo().String())
		reqchan <- strconv.Itoa(2)
	}

}

func OrgRequestStart(conns map[string]net.Conn, id string) {
	statime := time.Now().UnixNano() / 1e6
	orgchan := make(chan string)
	for user_id := range conns {
		OrgRequst(user_id, conns[user_id], id, orgchan)
		break
	}
	for x := range orgchan {
		if x == "1" {
			loginok++
		}

		endtime := time.Now().UnixNano() / 1e6
		andtime := endtime - statime
		log.Printf("登录完成！总数：%d|成功登录数：%d|失败登录数:%d|登录连接失败数:%d\n", len(conns), loginok, loginerr, loginconnerr)
		log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime, float64(andtime)/float64(len(conns)))
		break
	}
	return
}

func OrgRequst(user_id string, conn net.Conn, id string, orgchan chan string) {
	ulp := &youmai.IMGetOrgReq{
		OrgId: proto.String(id),
	}

	send(conn, ulp, user_id, youmai.COMMANDID_CID_ORG_LIST_REQ, 0, tcpCallback, orgchan)
}

//监听登录回调
func org_cb(rproto []byte, user_id string, i int, reqchan chan string) {
	rsp := &youmai.IMGetOrgRsp{}
	proto.Unmarshal(rproto, rsp)
	id := rsp.GetOrgId()
	fmt.Println("orgid:", id)
	org_list := rsp.GetOrgList()
	for i := range org_list {
		item := org_list[i]
		fmt.Println("id:", item.GetOrgId(), "name:", item.GetName(), "type:", item.GetType())
	}

}

//创建群
func CreateGroupStart(user_id string, conn net.Conn, members []string, name string, avator string, topic string) {
	statime := time.Now().UnixNano() / 1e6
	createGroupchan := make(chan string)

	go CreateGroupRequst(user_id, conn, members, name, avator, topic, createGroupchan)

	for x := range createGroupchan {
		if x == "1" {
			loginok++
		}

		endtime := time.Now().UnixNano() / 1e6
		andtime := endtime - statime

		log.Printf("总耗时：%dms\n", andtime)
		break
	}
	return
}

func CreateGroupRequst(user_id string, conn net.Conn, members []string, name string, avator string, topic string, reqchan chan string) {
	var member_list []*youmai.GroupMemberItem
	for i := range members {
		role := 2
		if members[i] == user_id {
			role = 0
		}
		member := &youmai.GroupMemberItem{
			MemberId:   proto.String(members[i]),
			UserName:   proto.String(members[i]),
			MemberRole: proto.Uint32(uint32(role)),
			MemberName: proto.String("test"),
		}
		member_list = append(member_list, member)
	}
	log.Println("size:", len(member_list))

	ulp := &youmai.GroupCreateReq{
		UserId:      proto.String(user_id),
		MemberList:  member_list,
		GroupName:   proto.String(name),
		GroupAvatar: proto.String(avator),
		Topic:       proto.String(topic),
	}

	send(conn, ulp, user_id, youmai.COMMANDID_CID_GROUP_CREATE_REQ, 0, tcpCallback, reqchan)
}

//监听登录回调
func CreateGroup_cb(rproto []byte, user_id string, i int, reach chan string) {
	rsp := &youmai.GroupCreateRsp{}
	proto.Unmarshal(rproto, rsp)
	id := rsp.GetGroupId()
	log.Println("group_id:", id)
	member_list := rsp.GetMemberList()
	log.Println("add success member: ", len(member_list))
	for i := range member_list {
		item := member_list[i]
		log.Println("user_id:", item.GetMemberId())
		break
	}
	reach <- "1"

}

//获取群列表
func GetGroupListStart(user_id string, conn net.Conn) {
	statime := time.Now().UnixNano() / 1e6
	ch := make(chan string)

	go GetGroupListRequest(user_id, conn, ch)

	<-ch
	endtime := time.Now().UnixNano() / 1e6
	andtime := endtime - statime

	log.Printf("总耗时：%dms\n", andtime)

	return
}

func GetGroupListRequest(userId string, conn net.Conn, reach chan string) {

	ulp := &youmai.GroupListReq{
		UserId: proto.String(userId),
	}

	send(conn, ulp, userId, youmai.COMMANDID_CID_GROUP_LIST_REQ, 0, tcpCallback, reach)
}

func getGroupListCb(rproto []byte, user_id string, i int, reach chan string) {
	rsp := &youmai.GroupListRsp{}
	proto.Unmarshal(rproto, rsp)
	groupList := rsp.GetGroupInfoList()
	log.Printf("group count: %d", len(groupList))
	for _, item := range groupList {
		log.Printf("groupId: %d,name:%s , count:%d,update_time:%v ,ownid:%s", item.GetGroupId(), item.GetGroupName(), item.GetGroupMemberCount(), item.GetInfoUpdateTime(), item.GetOwnerId())
	}
	reach <- "1"
}

//获取群成员
func GetGroupMemberStart(user_id string, conn net.Conn, groupId int32) {
	statime := time.Now().UnixNano() / 1e6
	ch := make(chan string)

	go GetGroupMemberRequest(user_id, conn, uint32(groupId), ch)

	<-ch

	endtime := time.Now().UnixNano() / 1e6
	andtime := endtime - statime

	log.Printf("总耗时：%dms\n", andtime)

	return
}

func GetGroupMemberRequest(userId string, conn net.Conn, groupId uint32, reach chan string) {

	ulp := &youmai.GroupMemberReq{
		UserId:  proto.String(userId),
		GroupId: proto.Uint32(groupId),
	}

	send(conn, ulp, userId, youmai.COMMANDID_CID_GROUP_MEMBER_REQ, 0, tcpCallback, reach)
}

func getGroupMemberCb(rproto []byte, user_id string, i int, reach chan string) {
	rsp := &youmai.GroupMemberRsp{}
	proto.Unmarshal(rproto, rsp)
	log.Println("groupId:", rsp.GetGroupId())
	memberList := rsp.GetMemberList()
	log.Println(" member count: ", len(memberList), "update_time:", rsp.GetUpdateTime(), "result:", rsp.GetResult())
	for _, item := range memberList {
		log.Println("memId:", item.GetMemberId(), "user_name:", item.GetUserName(), "member_name:", item.GetMemberName(), "role:", item.GetMemberRole())
	}
	reach <- "1"

}

//获取群信息
func GetGroupInfoStart(user_id string, conn net.Conn, groupId int32) {
	statime := time.Now().UnixNano() / 1e6
	ch := make(chan string)

	go GetGroupInfoRequest(user_id, conn, uint32(groupId), ch)

	<-ch

	endtime := time.Now().UnixNano() / 1e6
	andtime := endtime - statime

	log.Printf("总耗时：%dms\n", andtime)

	return
}

func GetGroupInfoRequest(userId string, conn net.Conn, groupId uint32, reach chan string) {

	ulp := &youmai.GroupInfoReq{
		UserId:     proto.String(userId),
		GroupId:    proto.Uint32(groupId),
		UpdateTime: proto.Uint64(0),
	}

	send(conn, ulp, userId, youmai.COMMANDID_CID_GROUP_INFO_REQ, 0, tcpCallback, reach)
}

func getGroupInfoCb(rproto []byte, user_id string, i int, reach chan string) {
	rsp := &youmai.GroupInfoRsp{}
	proto.Unmarshal(rproto, rsp)
	group_info := rsp.GetGroupInfo()
	log.Printf("groupId: %d,name:%s , count:%d,update_time:%v ,ownid:%s", group_info.GetGroupId(), group_info.GetGroupName(), group_info.GetGroupMemberCount(), group_info.GetInfoUpdateTime(), group_info.GetOwnerId())
	reach <- "1"

}

//修改群成员
func ChangeGroupMember(user_id string, conn net.Conn, groupId int32, member_list []string, flag int) {
	statime := time.Now().UnixNano() / 1e6
	ch := make(chan string)

	go ChangeGroupMemberRequest(user_id, conn, uint32(groupId), member_list, flag, ch)

	<-ch

	endtime := time.Now().UnixNano() / 1e6
	andtime := endtime - statime

	log.Printf("总耗时：%dms\n", andtime)

	return
}

func ChangeGroupMemberRequest(userId string, conn net.Conn, groupId uint32, members []string, flag int, reach chan string) {
	var member_list []*youmai.GroupMemberItem
	for i := range members {
		member := &youmai.GroupMemberItem{
			MemberId:   proto.String(members[i]),
			UserName:   proto.String(members[i]),
			MemberRole: proto.Uint32(uint32(2)),
			MemberName: proto.String("test"),
		}
		member_list = append(member_list, member)
	}
	var f youmai.GroupMemberOptType = youmai.GroupMemberOptType(flag)
	ulp := &youmai.GroupMemberChangeReq{
		UserId:     proto.String(userId),
		GroupId:    proto.Uint32(groupId),
		MemberList: member_list,
		Type:       &f,
	}

	send(conn, ulp, userId, youmai.COMMANDID_CID_GROUP_CHANGE_MEMBER_REQ, 0, tcpCallback, reach)
}

func ChangeGroupMemberRequestCb(rproto []byte, user_id string, i int, reach chan string) {
	rsp := &youmai.GroupMemberChangeRsp{}
	proto.Unmarshal(rproto, rsp)
	log.Println("groupId:", rsp.GetGroupId())
	memberList := rsp.GetMemberList()
	log.Println(" member count: ", len(memberList), "result:", rsp.GetResult())
	for _, item := range memberList {
		log.Println("memId:", item.GetMemberId())
	}
	reach <- "1"

}

//监听消息
func RequestMsgStart(conns map[string]net.Conn) {
	//statime:=time.Now().UnixNano()/1e6
	loginchan := make(chan string)
	//sleep:=util.CFG.Section("def").Key("connSleep").String()
	//s,_:= time.ParseDuration(sleep)
	i := 1
	for user_id := range conns {
		//time.Sleep(s)
		go requestMsg(user_id, conns[user_id], i, loginchan)
		i++
	}
	loginok := 0
	loginerr := 0
	loginconnerr := 0
	extcount := len(conns)
	for x := range loginchan {
		//log.Println(x+":"+strconv.Itoa(count))

		if x == "1" {
			loginok++
		} else if x == "2" {
			loginerr++
		} else if x == "4" {
			loginconnerr++
		}
		extcount--

		if extcount <= 0 { // 如果现有数据量为0
			extcount = len(conns)
			//endtime:=time.Now().UnixNano()/1e6
			//andtime:=endtime-statime

			//log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime,float64(andtime)/float64(len(conns)))
			//util.Prinyn("消息已全部接收完毕，是否继续接收？")
		}
		log.Printf("线程消息开始监听！连接数：%d|成功收消息数：%d|失败消息数：%d|连接丢失数:%d\n", len(conns), loginok, loginerr, loginconnerr)
	}

}
func requestMsg(user_id string, conn net.Conn, i int, loginchan chan string) {
	callbackfn(conn, user_id, i, tcpCallback, loginchan, false)
}

//监听消息回调
func request_msg_cb(rproto []byte, user_id string, i int, reqchan chan string) {
	personal := &youmai.ChatMsg{}
	error := proto.Unmarshal(rproto, personal)
	if error != nil {
		log.Println("解析proto错误：" + error.Error())
		reqchan <- strconv.Itoa(2)
		return
	}

	body := []byte(*personal.Data.MsgContent)
	//log.Println(body)
	var msg []MsgMap

	err := json.Unmarshal(body, &msg)
	if err != nil {
		log.Println(err.Error())
		reqchan <- strconv.Itoa(2)
	}
	//log.Println(msg)
	statime := msg[0].Timestamp
	bodymsg := msg[0].CONTENT_TEXT
	endtime := time.Now().UnixNano() / 1e6
	//log.Println(endtime)
	andtime := endtime - statime
	log.Printf("线程%d:%s收到消息=>%s|耗时：%dms\n", i, user_id, bodymsg, andtime)
	reqchan <- strconv.Itoa(1)
}

type MsgMap struct {
	CONTENT_TEXT string
	Timestamp    int64
}

var isrun int = 1
var numbe int = 0
var statime int64 = 0
var start bool = true
var loginok int = 0
var loginerr int = 0
var loginconnerr int = 0

func GetSendTimes() int {
	util.Prinyn("消息已全部发送完毕，是否持续发送")
	log.Printf("输入持续循环发送的次数")
	d := 0

	for {
		_, err := fmt.Scanf("%d", &d)
		if err != nil {

		} else {
			break
		}
	}
	return d

}

//发送消息
func SendMsgStart(recvers []string, conns map[string]net.Conn) {
	if isrun == 1 {
		numbe = 1
		statime = time.Now().UnixNano() / 1e6
	}
	loginchan := make(chan string)
	sleep := util.CFG.Section("def").Key("sendSleep").String()
	s, _ := time.ParseDuration(sleep)

	i := 0
	for user_id := range conns {
		time.Sleep(s)
		go sendMsg(user_id, recvers[i], conns[user_id], i, loginchan)
		i++
	}
	extcount := len(conns)

	for x := range loginchan {
		//log.Println(x+":"+strconv.Itoa(count))
		if x == "1" {
			loginok++
		}
		if x == "2" {
			loginerr++
		}
		if x == "4" {
			loginconnerr++
		}
		extcount--
		//log.Println(extcount)
		if extcount <= 0 { // 如果现有数据量为0，跳出循环
			//		endtime := time.Now().UnixNano() / 1e6
			//		andtime := endtime - statime
			//				log.Printf("发送完成！总数：%d|成功发送数：%d|失败发送数:%d|发送连接失败数:%d\n", len(conns), loginok, loginerr, loginconnerr)
			//					log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime, float64(andtime)/float64(len(conns)))
			//fmt.Println("numbe:", numbe)
			numbe--
			if numbe == 0 {
				log.Printf("send finish")
				endtime := time.Now().UnixNano() / 1e6
				andtime := endtime - statime
				log.Printf("发送完成！总数：%d|成功发送数：%d|失败发送数:%d|发送连接失败数:%d\n", len(conns), loginok, loginerr, loginconnerr)
				log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime, float64(andtime)/float64(loginok))
				numbe = GetSendTimes()
				start = true
				statime = time.Now().UnixNano() / 1e6
				loginok = 0
				loginerr = 0
				loginconnerr = 0
			}
			sleepRun := util.CFG.Section("def").Key("sendRunSleep").String()
			sr, _ := time.ParseDuration(sleepRun)
			//log.Printf("已发送%d次，持续发送间隔开始%s。。。。。。", isrun*len(conns), sleepRun)
			isrun++
			time.Sleep(sr)

			SendMsgStart(recvers, conns)
			//	fmt.Println("number:", numbe)
		}

	}

}

func sendMsg(sender string, recver string, conn net.Conn, i int, loginchan chan string) {

	//var sec = time.Now().UnixNano() / 1e9
	//var nm = (time.Now().UnixNano() % 1e9) / 1000
	statime := time.Now().UnixNano() / 1e6
	//statime := sec*1000 + nm
	msg := fmt.Sprintf("[{\"CONTENT_TEXT\":\"%s发来贺电\",\"Timestamp\":%d}]", sender, statime)
	//msg="[{\"CONTENT_TEXT\":\""+msg+"\"}]"
	data := &youmai.MsgData{}

	data.SrcUserId = proto.String(sender)
	data.DestUserId = proto.String(recver)

	data.MsgContent = proto.String(msg)

	ulp := &youmai.ChatMsg{}
	ulp.Data = data
	//	log.Printf("%s发往%s",kmp["msisdn"],sendUser["msisdn"])
	send(conn, ulp, sender, youmai.COMMANDID_CID_CHAT_BUDDY, i, tcpCallback, loginchan)

}

//发送群消息
func GroupMsgStart(user_id string, conn net.Conn, groupId int32) {

	if isrun == 1 {
		numbe = 1
		statime = time.Now().UnixNano() / 1e6
	}

	ch := make(chan string)

	go SendGroupMsgRequest(user_id, conn, uint32(groupId), ch)

	extcount := 1
	for x := range ch {
		//log.Println(x+":"+strconv.Itoa(count))
		if x == "1" {
			loginok++
		}
		if x == "2" {
			loginerr++
		}
		if x == "4" {
			loginconnerr++
		}
		extcount--
		if extcount <= 0 { // 如果现有数据量为0，跳出循环
			//		endtime := time.Now().UnixNano() / 1e6
			//		andtime := endtime - statime
			//				log.Printf("发送完成！总数：%d|成功发送数：%d|失败发送数:%d|发送连接失败数:%d\n", len(conns), loginok, loginerr, loginconnerr)
			//					log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime, float64(andtime)/float64(len(conns)))
			//fmt.Println("numbe:", numbe)
			numbe--
			if numbe == 0 {
				log.Printf("send finish")
				endtime := time.Now().UnixNano() / 1e6
				andtime := endtime - statime
				log.Printf("发送完成！总数：%d|成功发送数：%d|失败发送数:%d|发送连接失败数:%d\n", 1, loginok, loginerr, loginconnerr)
				log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime, float64(andtime)/float64(loginok))
				numbe = GetSendTimes()
				start = true
				statime = time.Now().UnixNano() / 1e6
				loginok = 0
				loginerr = 0
				loginconnerr = 0
			}
			sleepRun := util.CFG.Section("def").Key("sendRunSleep").String()
			sr, _ := time.ParseDuration(sleepRun)
			//log.Printf("已发送%d次，持续发送间隔开始%s。。。。。。", isrun*len(conns), sleepRun)
			isrun++
			time.Sleep(sr)

			GroupMsgStart(user_id, conn, groupId)
			//	fmt.Println("number:", numbe)
		}

		return
	}
}

var total int = 1000

func SendGroupMsgRequest(userId string, conn net.Conn, groupId uint32, reach chan string) {

	//var sec = time.Now().UnixNano() / 1e9
	//var nm = (time.Now().UnixNano() % 1e9) / 1000
	//statime := time.Now().UnixNano() / 1e6
	//statime := sec*1000 + nm
	//userId="4d0d2118-5443-4132-b982-4232ef4d6d01"
	msg := fmt.Sprintf("[{\"CONTENT_TEXT\":\"%s发来贺电%d\"}]", userId, total)
	total += 1
	//msg="[{\"CONTENT_TEXT\":\""+msg+"\"}]"
	data := &youmai.MsgData{}

	data.SrcUserId = proto.String(userId)
	data.GroupId = proto.Uint32(groupId)
	tp := youmai.SessionType_SESSION_TYPE_MULTICHAT
	data.SessionType = &tp

	data.MsgContent = proto.String(msg)

	ulp := &youmai.ChatMsg{}
	ulp.Data = data
	//	log.Printf("%s发往%s",kmp["msisdn"],sendUser["msisdn"])

	send(conn, ulp, userId, youmai.COMMANDID_CID_CHAT_GROUP, 0, tcpCallback, reach)

}

//发送消息回调
func send_msg_cb(rproto []byte, user_id string, i int, reqchan chan string) {
	loingack := &youmai.ChatMsg_Ack{}
	proto.Unmarshal(rproto, loingack)
	//log.Print(rproto)
	//youmai.ERRNO_CODE()
	if loingack.GetErrerNo() == 0 {
		//		log.Print("线程"+strconv.Itoa(i+1)+":"+user_id+"发送成功")
		reqchan <- strconv.Itoa(1)
	} else {
		log.Print("线程" + strconv.Itoa(i+1) + ":" + user_id + "发送失败" + loingack.GetErrerNo().String())
		reqchan <- strconv.Itoa(2)
	}
}

func tcpCallback(cmd int, rproto []byte, user_id string, i int, reach chan string) {

	if cmd == 102 {
		loing_cb(rproto, user_id, i, reach)
	} else if cmd == 125 {
		request_msg_cb(rproto, user_id, i, reach)
	} else if cmd == 0X5014 {
		send_msg_cb(rproto, user_id, i, reach)
	} else if cmd == 0x6112 {
		org_cb(rproto, user_id, i, reach)
	} else if cmd == int(youmai.COMMANDID_CID_GROUP_CREATE_RSP) {
		CreateGroup_cb(rproto, user_id, i, reach)
	} else if cmd == int(youmai.COMMANDID_CID_GROUP_LIST_RSP) {
		getGroupListCb(rproto, user_id, i, reach)
	} else if cmd == int(youmai.COMMANDID_CID_GROUP_MEMBER_RSP) {
		getGroupMemberCb(rproto, user_id, i, reach)
	} else if cmd == int(youmai.COMMANDID_CID_GROUP_CHANGE_MEMBER_RSP) {
		ChangeGroupMemberRequestCb(rproto, user_id, i, reach)
	} else if cmd == int(youmai.COMMANDID_CID_GROUP_INFO_RSP) {
		getGroupInfoCb(rproto, user_id, i, reach)
	} else {
		log.Printf("cmd:%d not find", cmd)
	}
}

func CreateGroup(userId string, members []string, groupId uint32) *bytes.Buffer {
	var member_list []*youmai.GroupMemberItem
	for i := range members {
		role := 2
		if members[i] == userId {
			role = 0
		}
		member := &youmai.GroupMemberItem{
			MemberId:   proto.String(members[i]),
			MemberRole: proto.Uint32(uint32(role)),
		}
		member_list = append(member_list, member)
	}
	ulp := &youmai.RDGroupMemberList{
		MemberList: member_list,
	}
	var sendbuf *bytes.Buffer
	ulpByte, _ := proto.Marshal(ulp)
	sendbuf = new(bytes.Buffer)
	binerr := binary.Write(sendbuf, binary.LittleEndian, ulpByte)
	if binerr != nil {
		fmt.Println("binary.Write failed:", binerr)

	}
	return sendbuf
}
