#include "IMClient.h"
#include <thread>
#include <string>
#include "redis_client.h"
#include <log_util.h>
#include <unistd.h>
#include <algorithm>
#include <iterator>
#include <set>
using namespace std;
#define IP   "192.168.17.130"
#define PORT  8003


RedisClient redis_client;
std::vector<string> g_users;
std::vector<string> g_send_users;
std::vector<string> g_recv_users;
IMClient client;

extern vector<int> vi;
extern vector<int> vii;

int main(int argc,char** argv) {
    if(argc<6){
        printf("please input com ip and port,usage: imtest comip comport redisip redisport  password\n");
        return 0;
    }
    client.init();
    printf("size:%d\n",vi.size());
    char* ip=argv[3];
    int port=atoi(argv[4]);
    char* auth=argv[5];
    redis_client.Init_Pool(ip,port,auth,1);
	std::vector<std::string> value;
	redis_client.GetIMUser(value);
    sort(value.begin(),value.end());
	LOGI("total user:%d", value.size());

	
	int pagesize;
    printf("pagesize:\n");
	scanf("%d", &pagesize);
	int pagenum;
    printf("start pagenum:\n");
	scanf("%d", &pagenum);
    printf("pagenum=%d\n",pagenum);

	g_recv_users=vector<string>(value.begin()+pagesize*(pagenum - 1), value.begin() + pagesize*pagenum);
	client.createGroup(g_recv_users, value[0],ip,port,auth);
	LOGD("sync success ,total users:%d", g_recv_users.size());

	//copy(g_users.begin() + pagesize*(pagenum - 1), g_users.begin() + pagesize*pagenum, back_inserter(g_recv_users));
	printf("recv user:%d", g_recv_users.size());
	printf("begin connection....\n");
	sleep(1);
	
	client.StartServer(argv[1], atoi(argv[2]), pagesize);
}
