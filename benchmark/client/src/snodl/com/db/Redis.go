package db

import (
	"github.com/go-redis/redis"
	"os"
	"log"
	"snodl/com/util"
)
var RedisClient *redis.Client

func RedisClientInit() *redis.Client{
	poolSize,_:= util.CFG.Section("redis").Key("PoolSize").Int()
	db,_:= util.CFG.Section("redis").Key("Db").Int()
	client := redis.NewClient(&redis.Options{
		Addr:     util.CFG.Section("redis").Key("Addr").String(),
		Password: util.CFG.Section("redis").Key("Password").String(), // no password set
		DB:       db,        // use default DB
		PoolSize:poolSize,
		ReadTimeout:1000*1000*1000*60,
		IdleTimeout:1000*1000*1000*3,
		IdleCheckFrequency:1000*1000*1000*1,
		//PoolTimeout:30000,
	})

	//测试连接
	err := client.Ping().Err()
	if err != nil {
		log.Println(err)
		os.Exit(404)
	}
	RedisClient=client

	return client
}
