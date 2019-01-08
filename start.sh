#!/bin/bash
#=============================
#ScriptName:ymservice
#ver:V3.0
#Date:20160803
#=============================
cd `dirname $0`
stty erase '^H'
#set program running environment
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../lib:/usr/local/lib
ulimit -c unlimited
#max connection num
ulimit -n 100000

LOGFILE=/var/log/ymservice
PATH1=$(pwd)
CONN=im_conn
DISPATCH=im_dispatch
BALANCE=im_loadbalance
LOGIN=im_login
BUDDY=im_buddy
GROUP=im_group
MSG=im_msg
MONITOR=im_monitor

PS_LIST1="im_monitor im_conn im_dispatch im_loadbalance  im_login im_buddy im_group im_msg"
PS_LIST2=(MONITOR BALANCE  CONN  DISPATCH  LOGIN  MSG  BUDDY  GROUP ALL)
CMD_LIST=(restart stop start)


#kill `ps -ef|grep 进程名 | grep -v grep|awk '{print $2}'` 
#screen -S TEST -p 0 -X stuff "ping www.baidu.com ^M"
#screen -dmS
   
restart(){
 running=`ps -ef |pgrep $1|wc -l`
   if [ $running -ge 1 ];then
       echo "$1 is runnging,stop $1" 
	   kill `ps -ef |pgrep $1`
	   fi	   
	  echo "Start $1" 
	  chmod a+x $1;nohup ./$1 >/dev/null &
          sleep 1
	  count=`ps -ef |grep \.\/$1|grep -v grep |wc -l`
      if [ $count -gt 0 ];then
      echo "$1 startup successfully "
      else
            echo "$1 startup failed "
      fi

}

stop(){
 running=`ps -ef |pgrep $1|wc -l`
   if [ $running -le 0 ];then
       echo "$1 was stoped" 
	   else 
	   echo "$1 is running,stop $1"
	   kill `ps -ef |pgrep $1`
	  count=`ps -ef |grep \.\/$1|grep -v grep |wc -l`
      if [ $count -le 0 ];then
      echo "$1 stop successfully "
      else
         echo "$1 stop failed "
      fi
      fi

}

start(){
 running=`ps -ef |pgrep $1|wc -l`
   if [ $running -ge 1 ];then
       echo "$1 is runnging now" 
	   else 
	   echo "$1 was stoped,start $1"
	  chmod a+x $1;nohup ./$1 >/dev/null &
          sleep 1
	  count=`ps -ef |grep \.\/$1|grep -v grep |wc -l`
      if [ $count -gt 0 ];then
      echo "$1 startup successfully "
      else
          echo "$1 startup failed "
      fi
      fi
}

ACTION=$1
PSNAME=$2

PSNAME_TMP=$(echo $PSNAME | tr [a-z] [A-Z])
   if [ $# -lt 2 ];then
   echo  "命令错误，正确用法：（$0 stop|start|restart  login| buddy |group | msg |monitor|balance|conn|dispatch |all）" 
    exit 1
   fi
   
   count3=`echo "${CMD_LIST[@]}" | grep -w "$ACTION" |wc -l`
	if [ $count3 -eq 0 ];then
    echo  "命令错误，正确用法：（$0 stop|start|restart  login| buddy |group | msg |monitor|balance|conn|dispatch|all）" 
		exit 1
	fi

   if [ -z "$PSNAME_TMP" ];then
       echo "参数为空，请输入一个进程名!!!"
	   exit 1
   fi
   
   count4=`echo "${PS_LIST2[@]}" | grep -w "$PSNAME_TMP" |wc -l`
    if [ $count4 -eq 0 ];then
      echo  "进程名不在列表里面，请重新输入!!!正确用法：（$0  stop|start|restart  login| buddy |group | msg |monitor|balance|conn|dispatch|all）"
	  exit 1
      fi
   
	 
if [ "$ACTION" = "restart" ];then
     
case  "$PSNAME_TMP"  in 

"CONN"  )
 $ACTION $CONN
 ;;  
 
 "DISPATCH"  )
 $ACTION $DISPATCH
 ;;
 
 "BALANCE"  )
 $ACTION $BALANCE
 ;;  
 
 "LOGIN")
 $ACTION $LOGIN
;; 

  "BUDDY")
 $ACTION $BUDDY
;;

 "GROUP")
 $ACTION $GROUP 
;;

  "MSG")
 $ACTION $MSG
;;

 "MONITOR")
 $ACTION $MONITOR
;;

 "ALL")
 $ACTION $MONITOR
 sleep 1
 $ACTION $BALANCE
 sleep 1
 $ACTION $CONN
 sleep 1      
 $ACTION $DISPATCH
 sleep 1
 $ACTION $LOGIN
 sleep 1
  $ACTION $MSG
 sleep 1
 $ACTION $BUDDY
 sleep 1
 $ACTION $GROUP
 sleep 1   
esac  

else
case  "$PSNAME_TMP"  in   
  
 "CONN"  )
 $ACTION $CONN
 ;;  
 
 "DISPATCH"  )
 $ACTION $DISPATCH
 ;;
 
 "BALANCE"  )
 $ACTION $BALANCE
 ;;  
 
 "LOGIN")
 $ACTION $LOGIN
;; 

  "BUDDY")
 $ACTION $BUDDY
;;

 "GROUP")
 $ACTION $GROUP 
;;

  "MSG")
 $ACTION $MSG
;;

 "MONITOR")
 $ACTION $MONITOR
;;

 "ALL")
 $ACTION $MONITOR
 sleep 1
 $ACTION $BALANCE
 sleep 1
 $ACTION $CONN
 sleep 1      
 $ACTION $DISPATCH
 sleep 1
 $ACTION $LOGIN
 sleep 1
  $ACTION $MSG
 sleep 1
 $ACTION $BUDDY
 sleep 1
 $ACTION $GROUP
 sleep 1   
esac 

fi
exit 0




