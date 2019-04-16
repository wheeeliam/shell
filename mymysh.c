// mysh.c ... a small shell
// Started by John Shepherd, September 2018
// Completed by William Chen (z5165219), September/October 2018

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>
#include <assert.h>
#include <fcntl.h>
#include "history.h"
#include <errno.h>


// This is defined in string.h
// BUT ONLY if you use -std=gnu99
//extern char *strdup(char *);

// Function forward references

void trim(char *);
int strContains(char *, char *);
char **tokenise(char *, char *);
char **fileNameExpand(char **);
void freeTokens(char **);
char *findExecutable(char *, char **);
int isExecutable(char *);
void prompt(void);

// Global Constants

#define MAXLINE 200

// Global Data

/* none ... unless you want some */
#define TRUE 1
#define FALSE 0

// Main program
// Set up enviroment and then run main loop
// - read command, execute command, repeat

int main(int argc, char *argv[], char *envp[])
{
	pid_t pid;   // pid of child process
	int stat;    // return status of child
	char **path; // array of directory names
	int cmdNo;   // command number
	int i;       // generic index

	// set up command PATH from environment variable
	for (i = 0; envp[i] != NULL; i++) {
		if (strncmp(envp[i], "PATH=", 5) == 0) break;
	}
	if (envp[i] == NULL)
		path = tokenise("/bin:/usr/bin",":");
	else
		// &envp[i][5] skips over "PATH=" prefix
		path = tokenise(&envp[i][5],":");
#ifdef DBUG
	for (i = 0; path[i] != NULL;i++)
		printf("path[%d] = %s\n",i,path[i]);
#endif

	// initialise command history
	// - use content of ~/.mymysh_history file if it exists

	cmdNo = initCommandHistory();

	// main loop: print prompt, read line, execute command
	char line[MAXLINE];
	prompt();
	while (fgets(line, MAXLINE, stdin) != NULL) {
		trim(line); // remove leading/trailing space

		// exit - terminate the shell
		if (strcmp(line, "exit") == 0) break;

		// empty string, continue
		if (strcmp(line, "") == 0) { prompt(); continue; }

		// double exclamation mark to repeat last command
		if (strcmp(line, "!!") == 0) {
			strcpy(line, getCommandFromHistory(cmdNo-1));
			trim(line);
			printf("%s\n", line);
			prompt();
		// else, single exclamation mark to execute a numbered commnad in hist
		} else if (line[0] == '!') {
			// collects the seq num in chars
			char charNum[MAXLINE];
			for (int i = 0; line[i] != '\0'; i++) {
				charNum[i] = line[i+1];
			}

			// transforms char to int
			int intNum = atoi(charNum);
			if (isdigit(line[i]) == FALSE || intNum <= 0) {
				printf("Invalid history substitution\n");
				prompt();
				continue;
			}

			// command is outside boundaries of history sequence
			if (getCommandFromHistory(intNum) == NULL) {
				printf("No command #%d\n", intNum);
				prompt();
				continue;
			// if not, print out the command from history
			} else {
				strcpy(line, getCommandFromHistory(intNum));
				trim(line);
				printf("%s\n", line);
			}
		}

		char **command;
		command = tokenise(line, " ");

		// building history and incrementing
		addToCommandHistory(line, cmdNo);
		cmdNo++;

		// fileNameExpand returns NULL if no wildcard
		// if no wildcard, does not enter if statement
		if (fileNameExpand(command) != NULL) {
			command = fileNameExpand(command);
		}

		// cd - change directories
		if (strcmp(command[0], "cd") == 0) {
			// change directory to whatever is after "cd"
			// cases: cd to executable, cd to unknown - error, cd home
			if (command[1] != NULL) {
				char dir[BUFSIZ] = {0};
				getcwd(dir, sizeof(dir));
				strcat(dir, "/");
				strcat(dir, command[1]);
				// executable is unknown/unable to be found
				if (chdir(dir) != 0) {
					printf("%s: No such file or directory\n", command[1]);
					freeTokens(command);
					prompt();
					continue;
				// otherwise is found
				} else {
					chdir(dir);
					char currDir[MAXLINE];
					printf("%s\n", getcwd(currDir, sizeof(currDir)));
				}
			// command line is just "cd"
			} else {
				chdir(getenv("HOME"));
				char currDir[MAXLINE];
				printf("%s\n", getcwd(currDir, sizeof(currDir)));
			}
			freeTokens(command);
			prompt();
			continue;
		}

		// pwd - print pathname of current working directory
		if (strcmp(line, "pwd") == 0) {
			char currDir[MAXLINE];
			printf("%s\n", getcwd(currDir, sizeof(currDir)));
			freeTokens(command);
			prompt();
			continue;
		}

		// history - display the last 20 commands + their sequence numbers
		if (strcmp(command[0], "h") == 0 || strcmp(command[0], "history") == 0) {
		    FILE *f = fopen(".mymysh_history", "w");
			showCommandHistory(f);
		    fclose(f);
		    freeTokens(command);
		    prompt();
		    continue;
		}

		// the parent shell process then waits for the child to complete
		if ((pid = fork()) > 0) {
			// the parent shell waits
			wait(&stat);
			printf("--------------------\n");
			printf("Returns %d\n", WEXITSTATUS(stat));
			freeTokens(command);
			prompt();
		} else if (pid == 0) {
			// the child shell process invokes the execute() function
			char *foundExec;
			foundExec = findExecutable(*command, path);

            // input redirection
            for (int j = 0; command[j] != NULL; j++) {
                if (strcmp(command[j], "<") == 0) {
					// < is next to an executable file, and then after is end of command line
                    if (command[j+2] == NULL) {
                        int in = open(command[j+1], O_RDONLY);
                        if (in < 0) {
                            perror("Input redirection");
                            exit(1);
                        }
                        // get path to open file descriptor
                        command[j] = NULL;
                        // replace stdout by the open file descriptor
                        dup2(in, 0);
                        printf("Running %s ...\n", foundExec);
                        printf("--------------------\n");
                        // execute the command, carrying new stdout across 
                        execve(foundExec, command, envp);
                        perror("execve failed");
                        exit(1);
                    }
                }
            }

			if (foundExec == NULL) {
				printf("Command not found\n");
			} else {
				printf("Running %s...\n", foundExec);
				printf("--------------------\n");

                // output redirection
                for (int k = 0; command[k] != NULL; k++) {
					// if > is found
                    if (strcmp(command[k], ">") == 0) {
						// > is next to an executable file, and then after is end of command line
                        if (command[k+2] == NULL) {
                            int out = open(command[k+1], O_CREAT | O_WRONLY | O_RDONLY | O_TRUNC);
                            if (out < 0) {
                                perror("Input redirection");
                                exit(1);
                            }
                            command[k] = NULL;
                            // replace stdout by the open file descriptor
                            dup2(out, 1);
							// execute the command, carrying new stdout across                            
							execve(foundExec, command, envp);
                            exit(1);
                        }
                    }
                }
				// depending on the envp of compile, it may be unable to execute certain files
				if (execve(foundExec, command, envp) == -1) {
					printf("%s: unknown type of executable file\n", foundExec);
					return -1;
				}
			}
			stat = 0;
			return stat;
		}
  	}

   freeTokens(path);
   saveCommandHistory();
   cleanCommandHistory();
   printf("\n");

   return(EXIT_SUCCESS);
}

char **fileNameExpand(char **tokens)
{
    // glob(3) - Linux Man Page
    //  typedef struct {
    //       size_t   gl_pathc;    /* Count of paths matched         */
    //       char   **gl_pathv;    /* List of matched pathnames.     */
    //       size_t   gl_offs;     /* Slots to reserve in gl_pathv.  */
    //  } glob_t;

	// name of the glob_t struct
    glob_t wildcard;
	// flag to verify whether or not there are wildcards in the string
	int flag = 0;
    char **newTokens = malloc(sizeof(char *) * MAXLINE);
	char command[MAXLINE];
	command[0] = '\0';

    for (int i = 0; tokens[i] != NULL; i++) {
		// whether or not these wildcard are indicated throughout command line
		if (strchr(tokens[i], '*') != NULL || strchr(tokens[i], '?') != NULL ||
			strchr(tokens[i], '[') != NULL || strchr(tokens[i], '~') != NULL) {
			
			// if so, flag is now 1 to show wildcard exists
			flag = 1;
			glob(tokens[i], GLOB_NOCHECK|GLOB_TILDE, 0, &wildcard);

			// if at least 1 path which is matched
			if (wildcard.gl_pathc > 0) {
				// goes through all the paths to add to command
				for (int j = 0; j < wildcard.gl_pathc; j++) {
					strcat(command, wildcard.gl_pathv[j]);
					strcat(command, " ");
				}
			}

			// free the struct containing all data
			globfree(&wildcard);
		} else {
			// if the path doesnt contain a wildcard, it must still be added
			strcat(command, tokens[i]);
			strcat(command, " ");
		}
	}

	// if wildcard is present, tokenise the new command line
	if (flag == 1) {
		trim(command);
		newTokens = tokenise(command, " ");
		return newTokens;
	// otherwise return NULL and use original command line
	} else {
		return NULL;
	}
}

// findExecutable: look for executable in PATH
char *findExecutable(char *cmd, char **path)
{
	char executable[MAXLINE];
	executable[0] = '\0';
	if (cmd[0] == '/' || cmd[0] == '.') {
		strcpy(executable, cmd);
		if (!isExecutable(executable))
			executable[0] = '\0';
	}
	else {
		int i;
		for (i = 0; path[i] != NULL; i++) {
			sprintf(executable, "%s/%s", path[i], cmd);
			if (isExecutable(executable)) break;
		}
		if (path[i] == NULL) executable[0] = '\0';
	}
	if (executable[0] == '\0')
	   	return NULL;
	else
	   	return strdup(executable);
}

// isExecutable: check whether this process can execute a file
int isExecutable(char *cmd)
{
	struct stat s;
	// must be accessible
	if (stat(cmd, &s) < 0)
		return 0;
	// must be a regular file
	//if (!(s.st_mode & S_IFREG))
	if (!S_ISREG(s.st_mode))
		return 0;
	// if it's owner executable by us, ok
	if (s.st_uid == getuid() && s.st_mode & S_IXUSR)
		return 1;
	// if it's group executable by us, ok
	if (s.st_gid == getgid() && s.st_mode & S_IXGRP)
		return 1;
	// if it's other executable by us, ok
	if (s.st_mode & S_IXOTH)
		return 1;
	return 0;
}

// tokenise: split a string around a set of separators
// create an array of separate strings
// final array element contains NULL
char **tokenise(char *str, char *sep)
{
	// temp copy of string, because strtok() mangles it
	char *tmp;
	// count tokens
	tmp = strdup(str);
	int n = 0;
	strtok(tmp, sep); n++;
	while (strtok(NULL, sep) != NULL) n++;
	free(tmp);
	// allocate array for argv strings
	char **strings = malloc((n+1)*sizeof(char *));
	assert(strings != NULL);
	// now tokenise and fill array
	tmp = strdup(str);
	char *next; int i = 0;
	next = strtok(tmp, sep);
	strings[i++] = strdup(next);
	while ((next = strtok(NULL,sep)) != NULL)
	strings[i++] = strdup(next);
	strings[i] = NULL;
	free(tmp);
	return strings;
}

// freeTokens: free memory associated with array of tokens
void freeTokens(char **toks)
{
	for (int i = 0; toks[i] != NULL; i++)
	free(toks[i]);
	free(toks);
}

// trim: remove leading/trailing spaces from a string
void trim(char *str)
{
	int first, last;
	first = 0;
	while (isspace(str[first])) first++;
	last  = strlen(str)-1;
	while (isspace(str[last])) last--;
	int i, j = 0;
	for (i = first; i <= last; i++) str[j++] = str[i];
	str[j] = '\0';
}

// strContains: does the first string contain any char from 2nd string?
int strContains(char *str, char *chars)
{
   for (char *s = str; *s != '\0'; s++) {
		for (char *c = chars; *c != '\0'; c++) {
	   		if (*s == *c) return 1;
		}
   }
   return 0;
}

// prompt: print a shell prompt
// done as a function to allow switching to $PS1
void prompt(void)
{
   	printf("mymysh$ ");
}
