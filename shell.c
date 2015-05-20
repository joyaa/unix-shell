/*
	UTSKRIFTER FÖR checkEnv?

	Run with make CFLAGS=-DSIGDET=1 to enable signal detection
 */

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
#include <sys/timeb.h> 


static int numArgs = 0;	//number of arguments (including command)
int pgid;	// Main process group ID
static int grepON = 0;	// grep enable
char **grepArgs;

char **tokenize_line(char*);
int exec_line(char**);
void checkEnv_normal();
void checkEnv_grep(char **);
int exec_pipe(int, char const**[]);
void exec_pipeProc(int, int, char* const[]);
void proc_ignoreInterrupt();
char* getline();
#ifdef SIGDET
void termination_handler(int);
#endif

int main(int argc, char **argv) {
	char *line;
	char **args;
	int pstatus, status; //parent and process status 
	pid_t pid;
	int *sig;
	
	pgid = getpid();		// get pid of the main process
	setpgid(pgid, pgid);	// 

	proc_ignoreInterrupt();	// ignore some signals
	
	char* detection = "polling";
	// Initialize signal detection handler for background processes
#ifdef SIGDET
	detection = "signal handler";
	struct sigaction action;	// customized signal action handler
	action.sa_handler = &termination_handler; 	// function to be called in case of recieving signal
	sigemptyset(&action.sa_mask);	// initializes the signalmask to empty (no signal will be masked - all signals will be received) 
	action.sa_flags = SA_NOCLDSTOP; // only for terminated children (no signal when childs stop)
	if (sigaction(SIGCHLD, &action, NULL) < 0)
		perror("sigaction");
#endif
	printf("BG process termination detection: %s. \n", detection);
	// Main command-line prompter and executioner
	while(1) {

#ifndef SIGDET

		// when using polling as background process termination detection
		if((pid = waitpid(-1, &status, WNOHANG)) > 0) { 
			// &status indicates change for process specified by pid
			if(WIFEXITED(status) || WIFSIGNALED(status)) {
				// the status indicates normal child termination or termination by non handled signal  
				printf("BG [%d] terminated\n", pid);
			}
		}
#endif

		printf("> ");
		line = getline();
		if(line[0] == '\0') // if empty input, continue
			continue;
		
		//tokenize input line
		args = tokenize_line(line);

		//execute the command with arguments
		pstatus = exec_line(args);
		
		//free allocated memory
		free(line);
		free(args);					
	}
}

/*
	Reads input from stdin. 
   	Maximum length of 80 chars.
 */
char* getline(void) {
	int bufsize = 8; 	//initial buffersize
	int charpos = 0;	//position of character in the input line
	int c; //int representation of character, for use with EOF

	char* buf = malloc(sizeof(char)*bufsize);	
	
	//if allocation failed
	if (!buf) {
		perror("malloc");
	}

	//keep reading characters
	while (1) {

		c = getchar(); // get char from stdin

		if (c == '\n' || c == EOF) {
			// end of line (or file), add null byte to end of line and return it
			buf[charpos] = '\0';
			return buf;
		} else {
			// character in line, add to the line
			buf[charpos] = c;
			++charpos;
		}

		//check maximum input length
		if (charpos >= 81 ) { //81 as one byte needed for ending string
			fprintf(stderr, "input line exceeds 80 chars \n");
			return buf;
		}

		//double buffersize if needed
		if (bufsize <= charpos) {
			//input line longer than the allocated space for line
			bufsize *= 2;
			buf = realloc(buf, bufsize);
			if(!buf){
				perror("realloc");
			}
		}
	}
}

/*
	Tokenizes input line by whitespace, into words.
	Returns pointer to a pointer to the tokens.
 */
char **tokenize_line(char *line) {
	int buffersize = 8, tokenIndex = 0;
	char **tokens = malloc(buffersize*sizeof(char*)); // 80 chars maximum handle	 
	char *token;
   	
	if(!tokens) {
		fprintf(stderr, "Tokens allocation fail");
		exit(EXIT_FAILURE);
	}	

	token = strtok(line, " ");	// returns pointer to first token

	while(token != NULL) {
		tokens[tokenIndex] = token;
		++tokenIndex;

		if(tokenIndex > buffersize) {
			// increase allocation for tokens
			buffersize *= 2;			
			tokens = realloc(tokens, buffersize * sizeof(char*));
			if(!tokens) {
				fprintf(stderr, "Tokens allocation fail");
				exit(EXIT_FAILURE);
			}
		}	
		token = strtok(NULL, " "); // pointer to the next token
	}
	tokens[tokenIndex] = NULL; // end array with NULL
	numArgs = tokenIndex;
	return tokens;
}

/* 
	Change current directory. No arguments changes to home directory.
 */
int cd_cmd(char **args) {
	const char *path = getenv("HOME"); // default new directory path
	if(numArgs > 1)	 {
		// directory path specified as argument
		path = args[1];
	}
	if(chdir(path) < -1)
		perror("cd failed");	
	return 1;
}

/*
	Kill all processes associated with the process ID (main shell process). 
*/
int exit_cmd(char **args) {
	if(args[1] == NULL) {
		// correct use of the exit command
		// send signal to terminate to every process in the process group whose ID is -pid (abs value of pid).
		if(kill(-pgid, SIGTERM) < 0) 
			exit(EXIT_FAILURE);
	} else { 
		printf("Error using exit. Usage: \"exit\"\n");	
		return 1;
	}	
	return 0;
}

/*
	Execute the command and arguments from the input.
	Separates foreground and background processes.
 */

int exec_line(char **args) {
	pid_t pid;
	int status, background = 0;
	int *sig;
	struct timeb  tv, tv1;
	int diff;

	//check if foreground or background process
	if(*args[numArgs-1] == '&') {
		//background process
		background = 1;
		args[numArgs-1] = NULL;	//remove the '&'
	} else {
		//foreground process, start counting time
		ftime(&tv);
	}
	
	//check if any of the built in commands
	if(strcmp(args[0], "cd") == 0) {
		return cd_cmd(args);
	} 
	if(strcmp(args[0], "exit") == 0) {
		return exit_cmd(args); 
	}
	if(strcmp(args[0], "checkEnv") == 0) {
		if(numArgs > 1)
			checkEnv_grep(args);
		else 
			checkEnv_normal();
		return 1;
	}

	//in parentprocess: fork returns pid of child 
	//in childprocess: fork returns 0 
	pid = fork();	

	if(pid == 0) {
		// childprocess
		if(background) {
			//background process
			/*set PGID of childprocess to pgid 
			  (join childprocess with terminal process) */
			if (setpgid(0, pgid) < 0)	
				perror("setpgid");
		} else {
			//foreground process
			pid = getpid();	
			if (setpgid(pid, pid) < 0)	//set PGID of childprocess to pid of childprocess
				perror("setpgid");

			/*makes process group of childprocess (pid) the
			  process group on terminal associated w STDIN_FILENO */ 
			if (tcsetpgrp(STDIN_FILENO, pid) < 0) 
				perror("tcsetpgrp");
			proc_ignoreInterrupt();	
		}	
		if(execvp(args[0], args) == -1)	//execvp only returns in case of error
			perror("fork error");
		exit(EXIT_FAILURE);	
	} else if(pid == -1) {
		perror("fork error");	
	} else {
		// parentprocess
		if(!background){
			//foreground process
			if (setpgid(pid, pid) < 0) //set PGID of parentprocess to pid of parentprocess
				perror("setpgid");

			// set current fg process pgid to "working" process group
			if (tcsetpgrp(STDIN_FILENO, pid) < 0)
				perror("tcsetpgrp");

			printf("FG [%d] forked\n", pid);
			waitpid(pid, &status, 0); //waiting for fg process w pid to terminate

			// set fg process group to pgid, i.e the originial process group
			if (tcsetpgrp(STDIN_FILENO, pgid) < 0)
				perror("tcsetpgrp");

			ftime(&tv1);
			
			diff = (int) (1000.0 * (tv1.time - tv.time)	+ (tv1.millitm - tv.millitm));	

			printf("FG [%d] terminated, runtime: %dms\n", pid, diff);
		} else { 
			//bg process
			printf("BG [%d] forked\n", pid);
			// set pgid of process w pid to original fg pg, to be able to terminate upon exit
			if (setpgid(pid, pgid) < 0)
				perror("setpgid");
		}
	}	
	return 1;
}

/*
	Executes the command "checkEnv" when 
	entered without arguments.
	checkEnv executes - printenv | sort | pager
 */
void checkEnv_normal(void) {
	struct timeb  tv, tv1;
	int diff;
	
	char *pagerType = getenv("PAGER"); //get type of pager

	//as arrays for execvp
	const char *printenv[] = {"printenv", NULL};
	const char *sort[] = {"sort", NULL};
	const char *pager[] = {"less", NULL}; //default pager less

	if(!(pagerType == NULL)) { //set pager to environment pager type if it isnt null
		pager[0] = pagerType;
		pager[1] = NULL;
	}
	const char** command[] = {printenv, sort, pager};
	pid_t pid;
    	int status;

		
	ftime(&tv);

	//execute checkEnv command in a new process
	if((pid = fork()) > -1) {
		if(pid == 0) {
			//childprocess
			if(exec_pipe(3, command) == -1) {
				pager[0] = "more"; 	// set pager to more if env pager and less failed
				pager[1] = NULL;
				const char** command[] = {printenv, sort, pager}; //update command
				if(exec_pipe(3, command))
					perror("checkEnv");
			} else {
				exit(EXIT_FAILURE);
			}
		} else {	
			//parentprocess
			
			printf("FG [%d] forked\n", pid);
			waitpid(pid, &status, 0);
			ftime(&tv1);
			
			diff = (int) (1000.0 * (tv1.time - tv.time)	+ (tv1.millitm - tv.millitm));	

			printf("FG [%d] terminated, runtime: %dms\n", pid, diff);
	
		}
	} else {
		perror("fork");
		exit(EXIT_FAILURE);
	}
}

/*
	Executes the command "checkEnv" when 
	entered with grep arguments
	checkEnv <arguments> executes - printenv | grep <arguments> | sort | pager
 */
void checkEnv_grep(char** args) {
	grepON = 1;
	args[0] = "grep"; // overwrite checkEnv w grep
	grepArgs = args;

	checkEnv_normal();
}

/* BEHÖVS???
	
 */
int run_last(char * const args[]) {
	return execvp(args[0], args);
}

/*
	Executes commands with pipes (in case of checkEnv)
 */
int exec_pipe(int n, char const** command[]) {
	int i;
	int in, fd[2]; //fs holds file descriptors of both ends, write into fd[1], fd[0] reads
	
	// First process gets input from fd 0 (STDIN_FILENO).
	in = 0;

	//execute the commands in command one at a time with pipes
	for(i = 0; i < (n-1); ++i) {

		if(pipe(fd) == -1) { //create pipe and store in fd
			perror("pipe");
			exit(EXIT_FAILURE);
		}

		if(grepON && i == 1) { // grep is the 2nd command (index 1)
			exec_pipeProc(in, fd[1], grepArgs);
		} else { // default
			exec_pipeProc(in, fd[1], (char* const*)command[i]);
		}

		// close write end of pipe, may now reuse the closed fd
		close(fd[1]);
		// the read end will be used by the next child to read from
		in = fd[0];
	}	
	if(in != 0)
		dup2(in, 0); // redirect 
	
	return run_last((char* const*)command[i]);
}

/*
	Copies and closes filedescriptors for use in next piped command.
	Also executes a piped command.
 */
void exec_pipeProc(int in, int out, char* const args[]) {
	pid_t pid;

	if((pid = fork ()) == 0) {
		//childprocess
		if(in != 0) {
			dup2(in, 0); //make fd specified by 0 copy of in fd
			close(in);
        }
		if(out != 1) {
			dup2(out, 1); //make fd specified by 1 copy of in out
			close(out);
		}
		if(execvp(args[0], args) == -1)	
			perror("fork error");		
		exit(EXIT_FAILURE);	
	}
}

/*
	Ignores the signal specified by the first argument in signal().
	In case of error when ignoring, give print error.
 */
void proc_ignoreInterrupt() {
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
/*
	Termination handler for use with signal detection.
 */
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
