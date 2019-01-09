#include "IMClient.h"

#include <log_util.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <signal.h>
#include <algorithm>
#include <sys/time.h>
#include <IM.Redis.pb.h>
#include <IM.Group.pb.h>
#include <redis.h>
using namespace com::proto::group;
using namespace com::proto::redis;
extern std::vector<string> g_recv_users;
extern IMClient client;
vector<int> vi;
void statistic(int signo){
    printf("recv sig:%d\n",signo);
    //sort(vi.begin(),vi.end(),[](int a,int b){return a>b;});

    int total_pkts=0;
    for(int i=0;i<10000;i++){
        total_pkts+=vi[i];
    }

    int num=0;
    for(int i=0;i<10000;i++){
        if(vi[i]==0){
            continue;
        }
        num+=vi[i];
        printf("< %d ms %d pkts(%0.2f%%)\n",i,num,((float)num/(float)total_pkts)*100);
    }
   // printf("<0 ms:%d ,1-10ms:%d ,10-100ms:%d,100-500ms:%d,500-1000ms:%d,1-2s:%d,2-3s:%d,>3s:%d\n",res[0],res[1],res[2],res[3],res[4],res[5],res[6],res[7]);

}
void count() {
    int sec_recv_pkt = 0;
    while (1) {
        sleep(1);
        int total = client.getRecvPkt();
         printf("total recv:%d,every recv:%d\n", total, total - sec_recv_pkt);
        sec_recv_pkt = total;
    }

}
IMClient::IMClient()
{
    m_num = 0;
    total_recv_pkts = 0;
    m_login = 0;
    signal(SIGINT,statistic);
}
void IMClient::init(){
    vi.resize(10000);
    for(int i=0;i<10000;i++){
        vi[i]=0;
    }

}
int IMClient::createGroup(std::vector<std::string> members,std::string owner,char* ip,int port,char* auth)
{
	Redis   redis;
	redis.connect(ip, port, auth, 7);
	RDGroupMemberList member_list;
	auto it = members.begin();
	for (it; it != members.end(); it++) {
		auto pItem = member_list.add_member_list();
		pItem->set_member_id(*it);
		pItem->set_member_role(2);
        pItem->set_user_name("test");
	}
	auto pItem = member_list.add_member_list();
	pItem->set_member_id(owner);
	pItem->set_member_role(0);
        pItem->set_user_name("test");

	char buf[1024 * 160];
	member_list.SerializeToArray(buf, member_list.ByteSize());

	std::string key = "gml_";
	key += std::to_string(members.size());
	const char *command[3];
	size_t vlen[3];

	command[0] = "set";
	command[1] = key.c_str();
	command[2] = buf;

	vlen[0] = 3;
	vlen[1] = key.length();
	vlen[2] = member_list.ByteSize();
	

	bool res = redis.query(command, sizeof(command) / sizeof(command[0]), vlen);
	if (!res) {
		return false;
	}
	LOGD("create success");
}
IMClient::~IMClient()
{
}

void IMClient::OnRecv(int _sockfd, PDUBase & _base)
{

}

void IMClient::OnRecv(int _sockfd, PDUBase * _base)
{
    switch (_base->command_id) {
        case CID_CHAT_BUDDY:
		case CID_CHAT_GROUP:
            chatMsg(_sockfd,*_base);
            break;
        case USER_LOGIN_ACK:
            loginAck(*_base);
            break;
        case BULLETIN_NOTIFY:
            printf("recv cmd:%d\n",_base->command_id);
            //bulletin(_sockfd,*_base);
            break;
    }
    delete _base;
}

void IMClient::OnConn(int _sockfd)
{
}

void IMClient::OnDisconn(int _sockfd)
{
    CloseFd(_sockfd);
}

void IMClient::OnSendFailed(PDUBase & _data)
{
}


void IMClient::loginAck(PDUBase & _base) {
    User_Login_Ack login_ack;
    if (!login_ack.ParseFromArray(_base.body.get(), _base.length)) {
        LOGE("IMChat_Personal包解析错误");
        //	ReplyChatResult(_sockfd, _base, ERRNO_CODE_DATA_SRAL);
        return;
    }
    int res = login_ack.errer_no();
    if (res == ERRNO_CODE_OK) {
        m_login++;
        printf("login success\n");
        if (m_login == g_recv_users.size()) {
            printf("%d users login success\n",g_recv_users.size());
            sleep(1);
            printf("begin recv msg\n");

            getchar();
            thread c(::count);
            c.detach();
        }

    }
}

void IMClient::chatMsg(int _sockfd,PDUBase & _base)
{
	ChatMsg  im;
    Device_Type device_type = DeviceType_UNKNOWN;

    if (!im.ParseFromArray(_base.body.get(), _base.length)) {

        //O	LOGE("IMChat_Personal包解析错误");
        //	ReplyChatResult(_sockfd, _base, ERRNO_CODE_DATA_SRAL);
        return;
    }

    if(!im.has_data()){
        printf("not find data\n");
    }
     MsgData data=im.data();
    struct timeval start;
    string content=data.msg_content();
    size_t pos=content.find("Timestamp");

    if(pos!=std::string::npos){
        string time=content.substr(pos+11,content.length()-pos-13);
        //printf("time:%s\n",time.c_str());
        long long t_ms=atoll(time.c_str());
    gettimeofday(&start,0);
        long long ms=((long long)start.tv_sec)*1000+start.tv_usec/1000;
        int delay=ms-t_ms;
        //printf("cur sec:%d nm:%d,send sec:%d,nm:%d\n",start.tv_sec,start.tv_usec,im.send_crc(),im.version());
        if(delay<0){
            delay=0;
        }
        if(delay>=10000){
            delay=9999;
        }
        vi[delay]=++vi[delay];
       // printf("delay:%d size:%d,cap:%d\n",delay,vi.size(),vi.capacity());
    }
    auto it=m_sock_userid.find(_sockfd);
    
    string dest_user_id(_base.terminal_token,sizeof(_base.terminal_token));
    string src_user_id=data.src_user_id();
    if(it->second!=dest_user_id){
        printf("send error uesr\n");

    }
    ++total_recv_pkts;
	ChatMsg_Ack ack;
    ack.set_msg_id(data.msg_id());
    ack.set_user_id(it->second);
    PDUBase _pack;
    memcpy(_pack.terminal_token,it->second.c_str(),sizeof(_pack.terminal_token));
    _pack.command_id = CID_CHAT_MSG_ACK;
    std::shared_ptr<char> body(new char[ack.ByteSize()]);
    ack.SerializeToArray(body.get(), ack.ByteSize());
    _pack.body = body;
    _pack.length = ack.ByteSize();
    Send(_sockfd, _pack);
}

void IMClient::OnLogin(int _sockfd)
{
    if (m_num > g_recv_users.size()) {
        printf("login users too manay ");
        return;
    }
    string user_id = g_recv_users[m_num];
    User_Login login;
    login.set_user_id(user_id);
	login.set_app_id("1234567");
    login.set_device_id("123456789");
        login.set_version(1);
    PDUBase _pack;
   memcpy( _pack.terminal_token,user_id.c_str(),sizeof(_pack.terminal_token));
    _pack.command_id = USER_LOGIN;
    std::shared_ptr<char> body(new char[login.ByteSize()]);
    login.SerializeToArray(body.get(), login.ByteSize());
    _pack.body = body;
    _pack.length = login.ByteSize();
    Send(_sockfd, _pack);
    ++m_num;
    m_sock_userid[_sockfd]= user_id;

}

int IMClient::getRecvPkt()
{
    return total_recv_pkts;
}

int IMClient::getLogin()
{

    return m_login;
}



