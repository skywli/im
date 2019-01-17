#!/bin/bash

#!/bin/bash

cmd=sadd
key=userIds
tmp=redis_data.txt
num=36899

redis_ip=127.0.0.1
redis_port=6379
redis_auth=123456

while [ $num -gt 0 ]; do
  # each command begins with *{number arguments in command}\r\n
  printf "*3\r\n" >> $tmp
  # for each argument, we append ${length}\r\n{argument}\r\n
  printf  "\$${#cmd}\r\n$cmd\r\n" >> $tmp
  printf  "\$${#key}\r\n$key\r\n" >> $tmp
  val=$(cat /proc/sys/kernel/random/uuid)
  printf "\$${#val}\r\n$val\r\n" >> $tmp
  let num=$num-1
done 

echo "All redis data is ok,begin to transfer to redis" 
cat redis_data.txt | redis-cli -h $redis_ip  -p $redis_port  -a  $redis_auth  --pipe
rm  $tmp
