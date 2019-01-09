#!/bin/bash
#=============================
#ScriptName:checkymservice
#ver:V3.0
#Writed by Hanlue
#Date:20160701
#=============================
cd `dirname $0`
stty erase '^H'
LOGFILE=/var/log/ymservice
PATH1=$(pwd)
EMAIL1=1328090688@qq.com
INFO="ymservice info:"
PS_LIST="im_monitor im_conn im_dispatch im_loadbalance  im_login im_buddy im_group im_msg"
#check and create logdirectory
if [ ! -d $LOGFILE ];then
mkdir /var/log/ymservice
fi

checkload(){
	  CpuLoad=`cat /proc/loadavg | awk {'print $1,$2,$3'}`
	  Mem=`free -m | awk {'print $6'} |tail -2`
	  PhyMem=`free | grep "Mem:" |awk '{print $2}'`
	  PhyMemused=`free | grep 'buffers/cache' | awk '{print $3}'`
	  MemPer=`awk 'BEGIN{printf"%.2f%\n",('$PhyMemused'/'$PhyMem')*100}'`
	  ConNum=`netstat -anp |grep 9951 |wc -l`
	  
}

		  
checkser(){

 running=`ps -ef |pgrep $1|wc -l`
   if [ $running -ge 1 ];then
      echo "$1 is runnging." 
	  else
      Time=`date +%F-%T`
      echo "$INFO $1 was stoped at $Time" | tee -a $LOGFILE/$1.log
      checkload
	  echo "System load info:CpuLoad:$CpuLoad;Memory usage:$MemPer;service_com connections:$ConNum at $Time". | tee -a $LOGFILE/$1.log
	  echo "$INFO $1 was stoped at $Time" | mail -s "Warning message from huxin im" $EMAIL1
	  
      echo "Starting $1"
     # chmod a+x $1;nohup ./$1 >/dev/null &
	./start.sh start ${1#*_}
	  count=`ps -ef |pgrep $1|wc -l`
	  if [ $count -gt 0 ];then
      Time=`date +%F-%T`
      echo "$INFO $1 startup successfully at $Time" | tee -a $LOGFILE/$1.log
	  echo  >>$LOGFILE/$1.log
	  echo "$INFO $1 startup successfully at $Time"| mail -s "Warning message from huxin im" $EMAIL1
	  
	  fi
	  
    fi
	 
}
  
    for psname in $PS_LIST;do
	checkser $psname
	done
exit 0
	
