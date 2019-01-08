#include <http.h>
#include <loadBalance.h>
#include <log_util.h>
#include<unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "config_file_reader.h"

LoadBalanceServer loadbalance_server;

int daemon() {
	int  fd;
	pid_t pid = fork();
	switch (pid) {
	case -1:
		fprintf(stderr, "fork() failed\n");
		return -1;

	case 0:
		break;

	default:
		exit(0);
	}

	if (setsid() == -1) {
		fprintf(stderr, "setsid() failed\n");
		return -1;
	}

	umask(0);

	fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		fprintf(stderr,
			"open(\"/dev/null\") failed\n");
		return -1;
	}

	if (dup2(fd, STDIN_FILENO) == -1) {
		fprintf(stderr, "dup2(STDIN) failed\n");
		return -1;
	}

	if (dup2(fd, STDOUT_FILENO) == -1) {
		fprintf(stderr, "dup2(STDOUT) failed\n");
		return -1;
	}

	if (fd > STDERR_FILENO) {
		if (close(fd) == -1) {
			fprintf(stderr, "close() failed\n");
			return -1;
		}
	}

	return 0;
}

int main() {
    if(access("../log",F_OK)==-1){
        mkdir("../log", 0766);
    }
    if(access("../log/loadbalance",F_OK)==-1){
       mkdir("../log/loadbalance", 0766);
    }
    setenv("project_dir","../log/loadbalance/",1);
	ConfigFileReader::getInstance()->init(CONF_LOADBALANCE_URL);
	daemon();
	signal(SIGPIPE, SIG_IGN);
	std::string level = ConfigFileReader::getInstance()->ReadString("loglevel");
	int l = atoi(level.c_str());
	initLog(CONF_LOG, l);
	
	std::string out_ip = ConfigFileReader::getInstance()->ReadString(CONF_CONN_OUT_IP);
	short port = ConfigFileReader::getInstance()->ReadInt(CONF_CONN_OUT_PORT);
	Http httpserver;
	if (httpserver.init(out_ip, port,1) == -1) {
		LOGE("httpserver init fail");
		return 0;
	}
	httpserver.start();

	if (loadbalance_server.init() == -1) {
		LOGD("LoadBalanceServer init fail");
		return -1;
	}
	//    std::thread count_thread=std::thread(count);
	//  count_thread.detach();

	loadbalance_server.start();
}
