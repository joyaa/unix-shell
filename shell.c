#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <time.h>

char **tokenize_line(char*);
int exec_line(char**);
static int numArgs = 0;

int main(int argc, char **argv) {
	char *line;
	char **args;
	int status;

	while(1) {
		line = readline("> "); // readline return is default malloc
		args = tokenize_line(line);
		status = exec_line(args);
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

	printf("%d", numArgs);
	if(*args[numArgs-1] == '&') {
		background = 1;
		args[numArgs-1] = NULL;	
	}
	else {
		start = time(0);
	}


	//for parentprocess: returns pid of child 
	//for childprocess: return 0 
	pid = fork();	
	
	if(pid == 0) {
		if(execvp(args[0], args) == -1)	//execvp only returns in case of error
			perror("fork error");		//print last error encountered on stderr
		exit(EXUT_FAILURE);	
	} else if(pid == -1) {
		perror("fork error");	
	} else {
		while((pid = waitpid(-1, &status, WUNTRACED)) > 0) {
			if(WIFEXITED(status) || WIFSIGNALED(status)) 
				stop = time(0);
				totalTime = difftime(start, stop);
				printf("Foreground process %d has been terminated, running time:%fms\n", pid, totalTime);
		}
	}
	return 1;
}
