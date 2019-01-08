package util

import (
	"fmt"
	"os"
	"log"
)

func Prinyn(msg string){
	log.Printf("%s：y；结束：n",msg)
	cmd:= ""
	for{
		fmt.Scanf("%s", &cmd)
		if cmd=="y" {
			break
		}else if cmd=="n" {
			os.Exit(2)
		}

	}
}
