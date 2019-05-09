#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>

#define MAX_CMD_ARG 10
#define MAX_CMD_GRP 10

const char *prompt = "myshell> ";

char* cmdgrps[MAX_CMD_GRP];
char* cmdvector[MAX_CMD_ARG];
char* cmdpipe[MAX_CMD_ARG];
char  cmdline[BUFSIZ];
int background;

void cd_(int argc, char** argv);
void fatal(char *str);
void execute_cmdline(char *cmd);
void execute_cmdgrp(char* cmdgrp);
int makelist(char *s, const char *delimiters, char** list, int MAX_LIST);
void zombie_handler();
void redirection(char *cmdline);
void execute_pipe(char *cmd);

int main(){
	int i = 0;
	int type = 0;
	int length = 0;
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = zombie_handler;
	act.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &act, NULL);

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	while (1) {

		fputs(prompt, stdout);
		fgets(cmdline, BUFSIZ, stdin);
		cmdline[strlen(cmdline) - 1] = '\0';

		execute_cmdline(cmdline);
		printf("\n");

	}
	return 0;
}

void cd_(int argc, char** argv) {
	int res;

	if (argc == 1) {
		res = chdir(getenv("HOME"));
		if (res == -1) {
			printf("error\n");
		}
	}
	else if (argc == 2) {
		res = chdir(argv[1]);
		if (res == -1) {
			printf("No directory\n");
		}
	}
	else {
		printf("too much argument\n");
	}
}

void fatal(char *str)
{
	perror(str);
	exit(1);
}

void zombie_handler(){
	waitpid(-1, NULL, WNOHANG);
}

void execute_cmdline(char* cmdline){
	int count = 0, cd_c;
	int i = 0, j, pid;
	count = makelist(cmdline, ";", cmdgrps, MAX_CMD_GRP);

	for (i = 0; i < count; ++i)
	{
		background = 0;
		for (j = 0; j < strlen(cmdgrps[i]); j++) {
			if (cmdgrps[i][j] == '&') {
				background = 1;
				cmdgrps[i][j] = '\0';
				break;
			}
		}

		if (cmdgrps[i][0] == 'c' && cmdgrps[i][1] == 'd') {
			cd_c = makelist(cmdgrps[i], " \t", cmdvector, MAX_CMD_ARG);
			cd_(cd_c, cmdvector);
		}
		else if (strcmp(cmdgrps[i], "exit") == 0) {
			printf("Bye.\n");
			exit(1);
		}
		else {
			switch (pid = fork()) {
			case -1: fatal("fork error");
			case  0:
				signal(SIGQUIT, SIG_DFL);
				signal(SIGINT, SIG_DFL);
				setpgid(0, 0);
				if (!background)
					tcsetpgrp(STDIN_FILENO, getpgid(0));
				execute_pipe(cmdgrps[i]);
				break;
			default:
				if (background) break;
				if (!background) {
					waitpid(pid, NULL, 0);
					tcsetpgrp(STDIN_FILENO, getpgid(0));
				}
				fflush(stdout);
			}
		}
	}
}

void execute_pipe(char *cmd) {
	int p[2];
	int count;
	int i;

	count = makelist(cmd, "|", cmdpipe, MAX_CMD_ARG);

	for (i = 0; i < count - 1; i++) {
		pipe(p);
		switch (fork()) {
		case -1: fatal("fork error");
		case 0:
			close(p[0]);
			dup2(p[1], 1);
			execute_cmdgrp(cmdpipe[i]);
		default:
			close(p[1]);
			dup2(p[0], 0);
		}
	}
	execute_cmdgrp(cmdpipe[i]);
}

void redirection(char *cmdline) {
	char *filename;
	int fd;
	int i;
	int length = strlen(cmdline);

	for (i = 0; i < length; i++) {
		if (cmdline[i] == '<') {
			filename = strtok(&cmdline[i + 1], " \t");
			fd = open(filename, O_RDONLY | O_CREAT, 0777);
			dup2(fd, 0);
			cmdline[i] = '\0';
		}
		if (cmdline[i] == '>') {
			filename = strtok(&cmdline[i + 1], " \t");
			fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
			dup2(fd, 1);
			cmdline[i] = '\0';
		}

	}
}

void execute_cmdgrp(char *cmdgrp) {
	int count = 0;

	redirection(cmdgrp);

	count = makelist(cmdgrp, " \t", cmdvector, MAX_CMD_ARG);
	execvp(cmdvector[0], cmdvector);

	fatal("exec error");
}

int makelist(char *s, const char *delimiters, char** list, int MAX_LIST) {
	int i = 0;
	int numtokens = 0;
	char *snew = NULL;

	if ((s == NULL) || (delimiters == NULL)) return -1;

	snew = s + strspn(s, delimiters);

	if ((list[numtokens] = strtok(snew, delimiters)) == NULL)
		return numtokens;

	if (list[numtokens] != NULL) {
		for (numtokens = 1; (list[numtokens] = strtok(NULL, delimiters)) != NULL; numtokens++) {
			if (numtokens == (MAX_LIST - 1)) return -1;
		}
	}

	if (numtokens > MAX_LIST) return -1;

	return numtokens;
}
