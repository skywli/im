#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include "config_file_reader.h"
#include <log_util.h>
#include <signal.h>
#include <errno.h>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include "buddy_server.h"

static char*   im_signal;
using namespace buddy;
static int get_options(int argc, char *const *argv)
{
	u_char     *p;
	int   i;

	for (i = 1; i < argc; i++) {

		p = (u_char *)argv[i];

		if (*p++ != '-') {
			fprintf(stderr, "invalid option: \"%s\"\n", argv[i]);
			return -1;
		}

		while (*p) {

			switch (*p++) {

			case 's':
				if (*p) {
					im_signal = (char *)p;

				}
				else if (argv[++i]) {
					im_signal = argv[i];

				}
				else {
					fprintf(stderr, "option \"-s\" requires parameter\n");
					return -1;
				}

				if (strcmp(im_signal, "stop") == 0
					|| strcmp(im_signal, "reload") == 0 || strcmp(im_signal, "restart") == 0)
				{

					goto next;
				}

				fprintf(stderr, "invalid option: \"-s %s\"\n", im_signal);
				return -1;

			default:
				fprintf(stderr, "invalid option: \"%c\"\n", *(p - 1));
				return -1;
			}
		}

	next:

		continue;
	}

	return 0;
}

static int handler_signal(const char* pid_file) {
	int fd = open(pid_file, O_RDONLY);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	pid_t  pid = 0;
	if (read(fd, &pid, sizeof(pid)) != sizeof(int)) {
		perror("read");
		return -1;
	}
	if (!strcmp(im_signal, "stop") || !strcmp(im_signal, "restart")) {
		if (kill(pid, SIGKILL) == -1) {
			fprintf(stderr, "close IMServer fail");
			return -1;
		}
	}
	close(fd);
	return 0;

}

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

int main(int argc, char *argv[]) {
    if(access("../log",F_OK)==-1){
        mkdir("../log", 0766);
    }
    if(access("../log/buddy",F_OK)==-1){
       mkdir("../log/buddy", 0766);
    }
    setenv("project_dir","../log/buddy/",1);
	
	ConfigFileReader::getInstance()->init(CONF_BUDDY_URL);
    std::string value= ConfigFileReader::getInstance()->ReadString("daemon");
	if (value == "on") {
		if (daemon() == -1) {
			return 0;
		}
	}
    signal(SIGPIPE,SIG_IGN);
	std::string level = ConfigFileReader::getInstance()->ReadString("loglevel");
	int l = atoi(level.c_str());
	initLog(CONF_LOG, l);

	BuddyServer * server= BuddyServer::getInstance();
	if (server->init() == -1) {
		LOGE("conn init fail");
		return -1;
	}
//    std::thread count_thread=std::thread(count);
  //  count_thread.detach();

	server->start();

    return 0;
}
