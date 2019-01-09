package client

import (

	"com/util"
	"encoding/json"
	"fmt"

	"io/ioutil"
	"log"
	"net"
	"net/http"
	"strconv"
	"time"
)

type CommetMap struct {
	msisdn string
	err    error
	conn   net.Conn
}

//建立连接池
func ConnectionPoolInit(usermap map[string]map[string]string) map[string]net.Conn {
	cp := make(map[string]net.Conn)
	statime := time.Now().UnixNano() / 1e6
	poolchan := make(chan CommetMap)
	i := 1
	sleep := util.CFG.Section("def").Key("connSleep").String()
	s, _ := time.ParseDuration(sleep)
	//log.Println(usermap)
	for t := range usermap {
		//fmt.Println(t+"="+strconv.Itoa(i))

		time.Sleep(s)
		go Connection(poolchan, i, usermap[t])
		i++
	}

	reqok := 0
	reqerr := 0

	extcount := len(usermap)
	for x := range poolchan {
		//log.Println(x+":"+strconv.Itoa(count))
		//log.Printf("xx0")
		if x.err != nil {
			reqerr++
		}
		if x.conn != nil {
			cp[x.msisdn] = x.conn
			reqok++
		}

		extcount--
		if extcount <= 0 { // 如果现有数据量为0，跳出循环
			break
		}

	}

	endtime := time.Now().UnixNano() / 1e6
	andtime := endtime - statime
	log.Printf("获取地址成功！总数：%d|成功数：%d|失败数:%d\n", len(usermap), len(usermap)-reqerr, reqerr)
	log.Printf("总耗时：%dms|平均耗时：%.2fms\n", andtime, float64(andtime)/float64(len(usermap)))
	return cp
}

type AddMap struct {
	Ip   string
	Port int
}

//起线程连接
func Connection(poolchan chan CommetMap, i int, msisdnMap map[string]string) {
	var cm = CommetMap{}
	cm.msisdn = msisdnMap["msisdn"]
	addr := ""

	url := ""
	url = util.CFG.Section("service").Key("HttpUrl").String() + "/api/v1/sum?userid=" + msisdnMap["uid"]
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
	log.Print("线程" + strconv.Itoa(i) + "获取地址成功:" + addr )

	poolchan <- cm
	//defer conn.Close()
}

