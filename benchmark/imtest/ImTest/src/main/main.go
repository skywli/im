package main

import (
	"com/client"
	"com/db"
	"com/util"
	"fmt"
	"log"
	//"math"
	"os"
	"sort"
	"strconv"
	"strings"
)

func main() {
	util.CfgInit()
	db.RedisClientInit()
	initTask()
}

var key string = "czy_us_9959f117-df60-4d1b-a354-776c20ffb8c7"

func initTask() {
	//选择db
	//pipeline:=db.RedisClient.Pipeline()
	//pipeline.Select(6)
	//log.Println(pipeline.Exec())

	user_list := db.RedisClient.SMembers(key).Val()
	sort.Strings(user_list)
	//总数量
	count := len(user_list)
	log.Println("总数据量:" + strconv.Itoa(count))
	//每页条数
	pagesize, _ := util.CFG.Section("def").Key("pagesize").Int()
	log.Println("当前每页数量:" + strconv.Itoa(pagesize))

	util.Prinyn("确认开始")

	users := user_list[:pagesize]
	fmt.Println("user[0]:", users[0])
	//建立连接
	cp := client.ConnectionPoolInit(users)
	util.Prinyn("确认开始登录")
	client.LoginStart(cp)

	for {

		//分支发送还接收
		log.Println("%s结束：n", "发送消息：1；接收消息：2；获取组织架构：3; 创建群：4；获取群列表：5 ；获取群成员列表；6；发送群消息：7 ;修改群成员：8;获取群信息：9")
		cmd := ""
		fmt.Scanf("%s\n", &cmd)
		if cmd == "1" {
			break
		} else if cmd == "2" {
			log.Println("开始接收消息。。。。。。")
			client.RequestMsgStart(cp)
		} else if cmd == "3" {
			log.Println("请输入组织架构id。。。。。。")
			id := ""
			fmt.Scanf("%s", &id)
			client.OrgRequestStart(cp, id)
		}else if cmd == "4" {
			log.Println("输入人数：")
			var num int
			name :="group_1"
			avatar :="http://www.baidu.com"
			topic :="test1"
			fmt.Scanf("%d", &num)
			members :=user_list[num:2*num]
			log.Println("输入名称：")
			fmt.Scanf("%s", &name)
			members=append(members,users[0])
			client.CreateGroupStart(users[0], cp[users[0]],members,name,avatar,topic)
		}else if cmd == "5" {
			var user_id string=""
			log.Println("输入user_id：")
			fmt.Scanf("%s",&user_id)
			client.GetGroupListStart(user_id, cp[users[0]])

		}else if cmd=="6" {
			var groupId int32
			log.Println("输入群id：")
			fmt.Scanf("%d", &groupId)
			client.GetGroupMemberStart(users[0], cp[users[0]],groupId)
		} else if cmd=="7" {
			var groupId int32
			log.Println("输入群id：")
			fmt.Scanf("%d", &groupId)
			client.GroupMsgStart(users[0], cp[users[0]],groupId)
		} else if cmd=="8"{
			var flag int=0
			log.Println("增加：1；删除：2")
			fmt.Scanf("%d",&flag)

			var groupId int32
			log.Println("输入群id：")
			fmt.Scanf("%d", &groupId)
			member_list :=user_list[2000:2003]
			fmt.Println(member_list[0])
			client.ChangeGroupMember(users[0], cp[users[0]],groupId,member_list,flag)

		}else if cmd=="9"{
			var groupId int32
			log.Println("输入群id：")
			fmt.Scanf("%d", &groupId)
			client.GetGroupInfoStart(users[0], cp[users[0]],groupId)
		} else{
			fmt.Println("unknown cmd:",cmd)
		}

	}

	//send
	recvers := user_list[pagesize : 2*pagesize]
	util.Prinyn("确认开始发送")
	client.SendMsgStart(recvers, cp)

	a := ""
	for {

		fmt.Println("输入exit退出")
		fmt.Scanf("%s", &a)
		if a == "exit" {
			os.Exit(1)
		}
	}

}

//获取分页
func getpaga(count int, pagesize int, page int) (int, int, int) {
	//起始页数
	d := 0
	for {
		_, err := fmt.Scanf("%d", &d)
		if err != nil {

		} else if d >= 1 && d <= page {
			break
		}
		//fmt.Println("分页从1开始"+err.Error())
	}

	//起始条数
	stacount := (d - 1) * pagesize
	//结束条数
	endcount := d * pagesize
	if count < endcount {
		endcount = count
	}
	//共%d条
	allcount := endcount - stacount
	return allcount, stacount, endcount
}

func redisUser(key string, rideschan chan map[string]string) {
	reredis := db.RedisClient.HGetAll(key)
	rerr := reredis.Err()
	var kmp map[string]string
	if rerr != nil {
		log.Println(rerr)
		kmp["s"] = "0"
		rideschan <- kmp
		return
	}
	kmp = reredis.Val()
	kmp["s"] = "1"
	kmp["msisdn"] = strings.SplitAfter(key, ":")[1]
	rideschan <- kmp
}
