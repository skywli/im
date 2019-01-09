#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <config_file_reader.h>
#include <log_util.h>
#include <signal.h>
#include <errno.h>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <client.h>
#include "linenoise.h"

struct helpEntries {
	const char* cmd;
    char* info;
};
helpEntries entries[] = {
	{"add"," node  sid ip port"},
	{"del"," node sid  nid"},
	{"stop"," node sid nid"},
	{"route"," set node sid nid [poll hash user slot] [user_id]"},
	{"slot","user_id"}
};

struct commandHelp {
	const char *name;
	const char *example;
	const char *summary;
} commandHelps[] = { { "add","add node sid nid ip port","add a node cluster" },
{ "del","del  node  sid  nid","del a node(include config) from cluster" },
{ "stop","stop node sid nid","stop a node " },
{ "route","route set node sid nid [poll/hash/slot/user] [user_id/slot]","set node route " },
{ "state","status","get cluster status" },
{"slot","slot user_id","calculate user slot"},
{ "quit","quit","exit the client" }
};

void helpCommand() {
	printf("	usage:\n");
	for (int i = 0; i < sizeof(commandHelps) / sizeof(commandHelp); i++) {
		printf("		cmd:%s\n", commandHelps[i].name);
		printf("		example:%s\n", commandHelps[i].example);
		printf("		summary:%s\n", commandHelps[i].summary);
		printf("\n");
	}
}

void cmdParse(char* buf, int& argc, char** argv) {
	int i = 0;
	char* p = buf;

	if (*p == '\n')
		return;
	while (*p++ == ' ');
	if (*p == '\n') {
		return;
	}
	argv[i++] = p - 1;

	while (*p) {
		if (*p == ' ') {
			*p = '\0';

			while (*(++p) == ' ');
			if (*p == '\n') {
				break;
			}
			else {
				argv[i++] = p;
			}
		}
		p++;
	}
//	*(p - 1) = '\0';
	argc = i;
}


void completion(const char *buf, linenoiseCompletions *lc) {
	if (buf[0] == 'a') {
		linenoiseAddCompletion(lc, "add node ");
	}
	else if (buf[0] == 'd') {
		linenoiseAddCompletion(lc, "del node ");
	}
	else if (buf[0] == 'r') {
		linenoiseAddCompletion(lc, "route  set node ");
	}
	else if (buf[0] == 's') {
		linenoiseAddCompletion(lc, "stop");
		linenoiseAddCompletion(lc, "stop node ");
	}
}

 char *hints(const char *buf, int *color, int *bold) {
	*color = 90;
	*bold = 0;
	for (int i = 0; i < sizeof(entries) / sizeof(helpEntries); i++) {
		if (!strcasecmp(buf, entries[i].cmd)) {

			return entries[i].info;
		}
	}
	
	return NULL;
}

int main(int argc, char *argv[]) {
    if(access("../log",F_OK)==-1){
        mkdir("../log", 0766);
    }
    if(access("../log/monitor_client",F_OK)==-1){
       mkdir("../log/monitor_client", 0766);
    }
    setenv("project_dir","../log/monitor_client/",1);
	
	ConfigFileReader::getInstance()->init(CONF_MONITOR_URL);
 
    signal(SIGPIPE,SIG_IGN);
	initLog(CONF_LOG,1);

	MonitorClient client;
	if (client.init() == -1) {
		LOGD("conn init fail");
		return -1;
	}

	char *line;
	char *prgname = argv[0];

	/* Set the completion callback. This will be called every time the
	* user uses the <tab> key. */
	linenoiseSetCompletionCallback(completion);
	linenoiseSetHintsCallback(hints);

	/* Load history from file. The history file is just a plain text file
	* where entries are separated by newlines. */
	//	linenoiseHistoryLoad("history.txt"); /* Load the history at startup */

	/* Now this is the main loop of the typical linenoise-based application.
	* The call to linenoise() will block as long as the user types something
	* and presses enter.
	*
	* The typed string is returned as a malloc() allocated string by
	* linenoise, so the user needs to free() it. */
	while ((line = linenoise("monitor--> ")) != NULL) {

		char* argv[1024] = { NULL };
		int argc = 0;
		/* Do something with the string. */
		if (line[0] != '\0' && line[0] != '/') {
			//printf("echo: '%s'\n", line);
			linenoiseHistoryAdd(line); /* Add to the history. */
			cmdParse(line, argc, argv);	//	linenoiseHistorySave("history.txt"); /* Save the history on disk. */
			if (argc == 1 && !strcmp(argv[0], "quit")) {
				break;
			}
			else if (argc == 1 && !strcmp(argv[0], "help")) {
				helpCommand();
			}
			else {
				int res = client.processCmd(argc, argv);
				if (res == 0) {
					client.read();
				}
			}
		}
		else if (!strncmp(line, "/historylen", 11)) {
			/* The "/historylen" command will change the history len. */
			int len = atoi(line + 11);
			linenoiseHistorySetMaxLen(len);
		}
		else if (line[0] == '/') {
			printf("Unreconized command: %s\n", line);
		}
		free(line);
	}
	return 0;
}
