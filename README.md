
##  Install 
#### 1 需要的库
libevent      
Protobuf3     
jemalloc    
leveldb    
log4cpp     


### 2 安装 
在工作目录下 依次执行：
- chmod 777  . /proto/gen.sh
- cmake -DCMAKE_INSTALL_PREFIX=你们需要安装的目录
- make && make install


### 3 启动
cd bin目录下执行

./start.sh  start all

停止
./start.sh  stop all

重启 ./start.sh restart all
