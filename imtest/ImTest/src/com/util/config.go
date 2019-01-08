package util

import (
	"github.com/go-ini/ini"
	"log"
	"os"
)

var CFG *ini.File
func CfgInit(){
	cfg, err :=ini.Load("config.ini")
	if err != nil {
		log.Println(err)
		os.Exit(2)
		return
	}
	CFG=cfg
}
