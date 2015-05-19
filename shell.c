// Run with make CFLAGS=-DSIGDET=1 to enable signal detection
#define _XOPEN_SOURCE 500
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>


static int numArgs = 0;	//number of arguments (including command)
int pgid;	// Main process group ID
static int grepON = 0;	// rep enable
char **grepArgs;

char **tokenize_line(char*);
int exec_line(char**);
void checkEnv_normal();
void checkEnv_grep(char **);
int exec_pipe(int, char const**[]);
void exec_pipeProc(int, int, char* const[]);
void proc_ignoreInterupt();
#ifdef SIGDET
void termination_handler(int);
#endif

int main(int argc, char **argv) {
	char *line;
	char **args;
	int pstatus, status;
	pid_t pid;
	int *sig;

	pgid = getpid();
	setpgid(pgid, pgid);

	proc_ignoreInterupt();	
	
#ifdef SIGDET
	struct sigaction action;
	action.sa_handler = &termination_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_NOCLDSTOP; // only for terminated children, not stopped
	if (sigaction(SIGCHLD, &action, NULL) < 0)
		perror("sigaction");
#endif

	while(1) {
#ifndef SIGDET
		if((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			if(WIFEXITED(status) || WIFSIGNALED(status)) {
				printf("BG [%d] terminated\n", pid);
			}
		}
#endif

			

		line = readline("> ");
		if(line[0] == '\0') // if empty input, just continue
			continue;
	
		args = tokenize_line(line);
		pstatus = exec_line(args);
		
		//free allocated memory
		free(line);
		free(args);					
	}
}

char **tokenize_line(char *line) {
	int buffersize = 8;
	char **tokens = malloc(buffersize*sizeof(char*)); // 80 chars maximum handle	
	int i = 0;
	char *token;
   	
	if(!tokens) {
		fprintf(stderr, "Tokens allocation fail");
		exit(EXIT_FAILURE);
	}	
	token = strtok(line, " ");
	while(token != NULL) {
		tokens[i] = token;
		++i;
		if(i > buffersize) {
			buffersize *= 2;			
			tokens = realloc(tokens, buffersize * sizeof(char*));
			if(!tokens) {
				fprintf(stderr, "Tokens allocation fail");
				exit(EXIT_FAILURE);
			}
		}	
		token = strtok(NULL, " ");
	}
	tokens[i] = NULL;
	numArgs = i;
	return tokens;
}

/* 
	Change current directory. No arguments changes to home directory.
 */
int cd_cmd(char **args) {
	const char *path = getenv("HOME"); // default new directory path
	if(numArgs > 1)	// directory path specified
		path = args[1];
	if(chdir(path) < -1)
		perror("cd failed");	
	return 1;
}

/*
	Kill all processes associated with the terminal. 
*/
int exit_cmd(char **args) {
	if(args[1] == NULL) {	// BÖR IGNORERA ARGUMENT?
		if(kill(-pgid, SIGTERM) < 0) // sig is sent to every process in the process group whose ID is -pid.
			exit(EXIT_FAILURE);
	} else { 
		printf("Error using exit. Usage: \"exit\"\n");	
		return 1;
	}	
	return 0;
}

int exec_line(char **args) {
	pid_t pid;
	int status, background = 0;
	time_t start, stop;	
	double totalTime;
	int *sig;

	//check if foreground or background process
	if(*args[numArgs-1] == '&') {
		//background process
		background = 1;
		args[numArgs-1] = NULL;	//remove the '&'
	} else {
		//foreground process
		start = time(0);
	}
	
	if(strcmp(args[0], "cd") == 0) {
		return cd_cmd(args);	// may change to args[1]
	} 

	if(strcmp(args[0], "exit") == 0) {
		return exit_cmd(args); 	// may change to args[1]
	}

	if(strcmp(args[0], "checkEnv") == 0) {
		if(numArgs > 1)
			checkEnv_grep(args);
		else 
			checkEnv_normal();
		return 1;
	}

	//in parentprocess: fork returns pid of child 
	//in childprocess: fork return 0 
	pid = fork();	
	if(pid == 0) {
		if(background) {
			if (setpgid(0, pgid) < 0)
				perror("setpgid");
		} else {
			pid = getpid(); //
			if (setpgid(pid, pid) < 0)
				perror("setpgid");
			if (tcsetpgrp(STDIN_FILENO, pid) < 0)
				perror("tcsetpgrp");
			proc_ignoreInterupt();	
		}	
		if(execvp(args[0], args) == -1)	//execvp only returns in case of error
			perror("fork error");		//print last error encountered on stderr
		exit(EXIT_FAILURE);	
	} else if(pid == -1) {
		perror("fork error");	
	} else {
		if(!background){
			if (setpgid(pid, pid) < 0)
			perror("setpgid");
			// set current fg process pgid to "working" process group
			if (tcsetpgrp(STDIN_FILENO, pid) < 0)
			perror("tcsetpgrp");

			printf("FG [%d] forked\n", pid);
			waitpid(pid, &status, 0);
			// set fg process group to pgid, i.e the originial process group
			if (tcsetpgrp(STDIN_FILENO, pgid) < 0)
			perror("tcsetpgrp");

			stop = time(0);
			totalTime = difftime(stop, start);
			printf("FG [%d] terminated, runtime: %.8fms\n", pid, totalTime);
		} else { 
			printf("BG [%d] forked\n", pid);
			// set pgid of process #pid to originial fg pg, to be able to terminate upon exit
			if (setpgid(pid, pgid) < 0)
				perror("setpgid");
		}
	}	
	return 1;
}

void checkEnv_normal(void) {
	time_t start, stop;	
	double totalTime;
	char *pagerType = getenv("PAGER");
	const char *printenv[] = {"printenv", NULL};
	const char *sort[] = {"sort", NULL};
	const char *pager[] = {"less", NULL};
	if(!(pagerType == NULL)) {
		pager[0] = pagerType;
		pager[1] = NULL;
	}
	const char** command[] = {printenv, sort, pager};
	pid_t pid;
    int status;
	start = time(0);
	if((pid = fork()) > -1) {
		if(pid == 0) {
			if(exec_pipe(3, command) == -1) {
				pager[0] = "more";
				pager[1] = NULL;
				const char** command[] = {printenv, sort, pager};
				exec_pipe(3, command); // will execute more if pager and less failed 
			} else {
				exit(EXIT_FAILURE);
			}
		} else {	
			waitpid(pid, &status, 0);
			stop = time(0);
			totalTime = difftime(stop, start);
			printf("Runtime:%.4fs\n", totalTime);
		}
	} else {
		perror("fork");
		exit(EXIT_FAILURE);
	}
}
int run_last(char * const args[]) {
	return execvp(args[0], args);
}

int exec_pipe(int n, char const** command[]) {
	int i;
	pid_t pid;
	int in, fd[2];
	
	// First process gets input from fd 0.
	in = 0;

	for(i = 0; i < (n-1); ++i) {
		if(pipe(fd) == -1) {
			perror("pipe");
			exit(EXIT_FAILURE);
		}
		if(grepON && i == 1) {
			exec_pipeProc(in, fd[1], grepArgs);
		} else { // default
		exec_pipeProc(in, fd[1], (char* const*)command[i]);
		}
		// close write end of pipe, child will write here
		close(fd[1]);
		// the read end will be used by the next child to read from
		in = fd[0];
	}	
	if(in != 0)
		dup2(in, 0); // redirect 
		
	return run_last((char* const*)command[i]);
}

void exec_pipeProc(int in, int out, char* const args[]) {
	pid_t pid;

	if((pid = fork ()) == 0) {
		if(in != 0) {
			dup2(in, 0);
			close(in);
        }
		if(out != 1) {
			dup2(out, 1);
			close(out);
		}
		if(execvp(args[0], args) == -1)	//execvp only returns in case of error
			perror("fork error");		//print last error encountered on stderr
		exit(EXIT_FAILURE);	
	}
}

void checkEnv_grep(char** args) {
	grepON = 1;
	args[0] = "grep";
	grepArgs = args;

	checkEnv_normal();
}

void proc_ignoreInterupt() {
	if(signal(SIGTTOU, SIG_IGN) == SIG_ERR)
				perror("sigttou");
	if(signal(SIGINT, SIG_IGN) == SIG_ERR)
				perror("sigint");
	if(signal(SIGQUIT, SIG_IGN) == SIG_ERR)
				perror("sigquit");
	if(signal(SIGTSTP, SIG_IGN) == SIG_ERR)
				perror("sigtstp");
	if(signal(SIGTTIN, SIG_IGN) == SIG_ERR)
				perror("sigttin");
}

#ifdef SIGDET 
void termination_handler(int signum) {
    int status;
    pid_t pid;
	if((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		if(WIFEXITED(status) || WIFSIGNALED(status)) {
			printf("BG [%d] terminated\n> ", pid);
		}
	}
}
#endif
