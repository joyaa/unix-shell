#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <time.h>
#include <sys/types.h>

static int numArgs = 0;	//number of arguments (including command)

char **tokenize_line(char*);
int exec_line(char**);

int main(int argc, char **argv) {
	char *line;
	char **args;
	int status;
	pid_t pid;

	while(1) {
		printf("PID%i", pid);
		while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			printf("Sug balle1");
			if(WIFEXITED(status) || WIFSIGNALED(status)) {
				printf("Process %d has been terminated\n", pid);
			}
		}
		line = readline("> "); // readline return is default malloc
		args = tokenize_line(line);
		status = exec_line(args);
		while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			printf("Sug balle2");
			if(WIFEXITED(status) || WIFSIGNALED(status)) {
				printf("Process %d has been terminated\n", pid);
			}
		}


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

int exec_line(char **args) {
	pid_t pid;
	int status, background = 0;
	time_t start, stop;	
	double totalTime;

	//check if foreground or background process
	if(*args[numArgs-1] == '&') {
		//background process
		background = 1;
		args[numArgs-1] = NULL;	//remove the '&'
	}
	else {
		//foreground process
		start = time(0);
	}


	//in parentprocess: fork returns pid of child 
	//in childprocess: fork return 0 
	pid = fork();	
	if(pid == 0) {
		if(execvp(args[0], args) == -1)	//execvp only returns in case of error
			perror("fork error");		//print last error encountered on stderr
		exit(EXIT_FAILURE);	
	} else if(pid == -1) {
		perror("fork error");	
	} else {
		printf("pid pid pid pid%i", pid);
		if(!background){
			waitpid(-1, &status, 0);
			stop = time(0);
			totalTime = difftime(stop, start);
			printf("Running time:%.4fms\n", totalTime);
		}
		while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			printf("Sug balle3");
			if(WIFEXITED(status) || WIFSIGNALED(status)) {
				printf("Process %d has been terminated\n", pid);
			}
		}
	}	
	return 1;
}
