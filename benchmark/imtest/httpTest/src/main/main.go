package main

import (
	"com/db"
	"strconv"
	"log"
	"com/client"
	"fmt"
	"os"
	"math"
	"com/util"
	"strings"
)

func main() {
	util.CfgInit()
	db.RedisClientInit()
	initTask()
}


func initTask() {
	//选择db
	//pipeline:=db.RedisClient.Pipeline()
	//pipeline.Select(6)
	//log.Println(pipeline.Exec())

	resMap:=db.RedisClient.Keys("ImUser:*").Val()
	//总数量
	count:=len(resMap)
	log.Println("总数据量:"+strconv.Itoa(count))
	//每页条数
	pagesize,_:= util.CFG.Section("def").Key("pagesize").Int()

	var pagefloat float64 =float64(count)/float64(pagesize)
	//总页数
	page:=int(math.Ceil(pagefloat))
	log.Println("当前每页数量:"+strconv.Itoa(pagesize))

	log.Printf("总页数: %d\n", page)

	log.Printf("输入起始分页%d-%d\n",1,page)
	allcount,stacount,endcount:=getpaga(count,pagesize,page)


	log.Printf("起至条数: %d-%d,共%d条\n", stacount,endcount,allcount)
	util.Prinyn("确认开始")
	log.Printf("请稍后从redis取用户数据（%d).....\n",allcount)
	usermap:=getuser(resMap,stacount,endcount,allcount)

	log.Printf("得到用户数据（%d）个\n",len(usermap))
	util.Prinyn("确认开始建立连接")

	//建立连接
	client.ConnectionPoolInit(usermap)


	a := ""
	for{

		fmt.Println("输入exit退出")
		fmt.Scanf("%s", &a)
		if(a == "exit"){
			os.Exit(1)
		}
	}

}
//获取分页
func getpaga(count int,pagesize int,page int)(int,int,int){
	//起始页数
	d:= 0
	for{
		_,err:=fmt.Scanf("%d", &d)
		if err != nil {

		}else if d>=1 && d<=page {
			break
		}
		//fmt.Println("分页从1开始"+err.Error())
	}

	//起始条数
	stacount:=(d-1)*pagesize
	//结束条数
	endcount:=d*pagesize
	if count<endcount{
		endcount=count
	}
	//共%d条
	allcount:=endcount-stacount
	return allcount,stacount,endcount
}
func getuser(resMap []string,stacount int,endcount int,allcount int) map[string]map[string]string{
	usermap:=make(map[string]map[string]string)
	extcount:=allcount
	rideschan := make(chan map[string]string)
	for i:=stacount;i<endcount;i++{
		//fmt.Println(resMap[i]+"="+strconv.Itoa(i))
		go redisUser(resMap[i],rideschan)
	}

	for x := range rideschan{
		//log.Println(x+":"+strconv.Itoa(count))
		if x["s"]=="1"{
			usermap[x["msisdn"]]=x
		}
		if x["s"]=="0"{

		}
		extcount--
		if extcount <= 0 { // 如果现有数据量为0，跳出循环
			break
		}
	}
	return usermap
}
func redisUser(key string, rideschan chan map[string]string){
	reredis:=db.RedisClient.HGetAll(key)
	rerr:= reredis.Err()
	var kmp map[string]string
	if rerr!=nil{
		log.Println(rerr)
		kmp["s"]="0"
		rideschan<-kmp
		return
	}
	kmp= reredis.Val()
	kmp["s"]="1"
	kmp["msisdn"]=strings.SplitAfter(key, ":")[1]
	rideschan<-kmp
}