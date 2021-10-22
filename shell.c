#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<readline/readline.h>
#include<readline/history.h>

#include <dirent.h>
#include <errno.h>

#define MAXCOM 1000 // max number of letters to be supported
#define MAXLIST 100 // max number of commands to be supported
  
// Clearing the shell using escape sequences
#define clear() printf("\033[H\033[J")

// Using this for getcwd
#define ARG_MAX_LEN 4096
#define ARG_MAX 64
#define MAX_BG_JOBS 64
#define NUM_BUILTIN_CMDS 8

#define IS_WHITESPACE(c) (c == ' ' || c == '\t')
// Any control, operator, or pipeline char is treated as an arg string
#define IS_ALLOWED(c) (c > 32 && c < 127)
// #define IS_CONTROL(c) ()
// #define IS_PIPELINE(c) ()
// #define IS_OPERATOR(c) ()
// #define IS_UNSUPPORTED(c) ()

// TODO List
//
// # movetodir directory  (DONE)
//    do not use chdir()
//
// # whereami (DONE)
//
// # history[-c] (DONE)
//
// # byebye (DONE)
//
// # replay number (BIG OOF)
//
// # start program [parameters]
//
// # background program [parameters]
//
// # dalek PID
//
// Extra credit...

typedef int (*CmdFunc)(char **argv);

typedef struct Process Process;
struct Process 
{
	// Chained processes would result one piping to another -- feature to add
	// later.
  Process *next;
	CmdFunc func;
  int pid;
  char argv[ARG_MAX][ARG_MAX_LEN];
};

typedef struct Job
{
  char cmd[ARG_MAX_LEN];
  Process *first_process;
  int stdin;
  int stdout;
  int stderr;
} Job;

typedef struct Shell
{
  char cmdHist[MAXLIST][1024];
  char currentdir[ARG_MAX_LEN]; // Current directory path
  char mainDir[ARG_MAX_LEN];
  
  int cmdCnt;
	Job *bgjobs[MAX_BG_JOBS];
} Shell;

typedef struct BuiltinCmd {
	char *cmd_name;
	CmdFunc func;
} BuiltinCmd;

enum ParseStatus {PARSE_OK=0, PARSE_INVALID_CHAR=1, PARSE_INVALID_CMD=2};

void init_job(Job *j);
void launch_job(Job *j);
void init_process(Process *p);
void launch_process(Process *p, int infile, int outfile, int errfile, int foreground);
int parse(Job *j, char *cmd);

int movetodir(char **argv);
int whereami(char **argv);
int history(char **argv);
int byebye(char **argv);
int replay(char **argv);
int start(char **argv);
int background(char **argv);
int dalek(char **argv);

static const BuiltinCmd builtin_cmds[] = {
	{"movetodir", movetodir},
	{"whereami", whereami},
	{"history", history},
	{"byebye", byebye},
	{"replay", replay},
	{"start", start},
	{"background", background},
	{"dalek", dalek},
	{NULL, NULL}
};


void init_job(Job *j)
{
	// Just making sure that the string is empty
	j->cmd[0] = '\0';	
	j->first_process = NULL;
	j->stdin = STDIN_FILENO;
	j->stdout = STDOUT_FILENO;
	j->stderr = STDERR_FILENO;
}

void init_process(Process *p)
{
	p->next = NULL;
	p->pid = 0;
}

CmdFunc parse_cmd(char *cmd_name)
{
	BuiltinCmd bi_cmd;		
	int i = 0;

	while ((bi_cmd = builtin_cmds[i]).cmd_name != NULL) {
		if (strcmp(bi_cmd.cmd_name, cmd_name) == 0)
			return bi_cmd.func;

		i++;
	}
	
	return NULL;
}

// cmd is a null or \n terminated string
int parse(Job *j, char *cmd)
{
	int arg = 0;
	int arg_len = 0;
	int parsing_arg = 0;
	int reached_end = 0;
	char c;
	int i;
	Process *proc = j->first_process;
	for (i = 0; i < ARG_MAX_LEN; i++) {
		c = cmd[i];

		if (c == '\0' || c == '\n')	
			reached_end = 1;	/* Need to finish parsing arg */
		
		j->cmd[i] = c;
		if (IS_WHITESPACE(c) || reached_end) {
			// Done parsing arg
			if (parsing_arg) {
				proc->argv[arg][arg_len] = '\0';
				printf("parsed argv: '%s'\n", proc->argv[arg]);

				if (arg == 0) {
					if ((proc->func = parse_cmd(proc->argv[0])) == NULL)
						return PARSE_INVALID_CMD;
				}
				parsing_arg = 0;
				arg_len = 0;
				arg++;
			}	

			if (reached_end)
				break;
		}
		else if (IS_ALLOWED(c)) {
			parsing_arg = 1;
			proc->argv[arg][arg_len] = c;
			arg_len++;
		}
		else {
			return PARSE_INVALID_CHAR;
		}
	}
	j->cmd[i] = '\0';
	
	return PARSE_OK;
}

void launch_job(Job *j)
{
	// Not supporting pipes atm
	Process *proc = j->first_process;	

	int pid = fork();
	if (pid < 0) {
		// Error
	}
	if (pid == 0) {
		// Child process
		launch_process(proc, j->stdin, j->stdout, j->stderr, 1);
	}
	else {
		// Parent process
	}

}

// Greeting shell during startup
void init_shell(Shell* shelly)
{
	clear();
  
  // Read history file
	char c;
  char str[100];
  char cmdHistTemp[MAXLIST][1024];
  int cnt = 0;
  int i = 0;
  FILE *file;
  file = fopen("hist", "rt");
  if (file) {
    while ((c=fgetc(file)) != EOF) {
      //shelly->cmdHist[1] = c;
      if (c != '\n')
        str[i++] = c;
      else
      {
        strcpy(cmdHistTemp[cnt++], str);
        memset(str, 0, sizeof(str));
        i=0;
      }
    }
    fclose(file);
  }
  while (cnt!=-1) {
    strcpy(shelly->cmdHist[shelly->cmdCnt++],cmdHistTemp[cnt--]);
  }
  
  
	printf("\n\n\n\n******************"
			"************************");
	printf("\n\n\n\t****The Shell to End all Shells****");
	printf("\n\n\t-hope I don't break anything-");
	printf("\n\n\n\n*******************"
			"***********************");
	char* username = getenv("USER");
	printf("\n\n\nThe Supreme Ruler is: @%s", username);
	printf("\n");
	sleep(1);
	//clear();
}
  
// Function where the system command is executed
void execArgs(char** parsed)
{
	// Forking a child
	pid_t pid = fork(); 

	if (pid == -1) {
		printf("\nFailed forking child..");
		return;
	} 
	else if (pid == 0) {
		if (execvp(parsed[0], parsed) < 0) {
				printf("\nCould not execute command..");
		}
		exit(0);
	} 
	else {
		// waiting for child to terminate
		wait(NULL); 
		return;
	}
}
  
// Function where the piped system commands is executed
void execArgsPiped(char** parsed, char** parsedpipe)
{
    // 0 is read end, 1 is write end
    int pipefd[2]; 
    pid_t p1, p2;
  
    if (pipe(pipefd) < 0) {
        printf("\nPipe could not be initialized");
        return;
    }
    p1 = fork();
    if (p1 < 0) {
        printf("\nCould not fork");
        return;
    }
  
    if (p1 == 0) {
        // Child 1 executing..
        // It only needs to write at the write end
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
  
        if (execvp(parsed[0], parsed) < 0) {
            printf("\nCould not execute command 1..");
            exit(0);
        }
    } else {
        // Parent executing
        p2 = fork();
  
        if (p2 < 0) {
            printf("\nCould not fork");
            return;
        }
  
        // Child 2 executing..
        // It only needs to read at the read end
        if (p2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            if (execvp(parsedpipe[0], parsedpipe) < 0) {
                printf("\nCould not execute command 2..");
                exit(0);
            }
        } else {
            // parent executing, waiting for two children
            wait(NULL);
            wait(NULL);
        }
    }
}

// Change directory builtin
// void movetodir(char* parsed, Shell* shelly)
// {
//   struct dirent *dp;
//   char parsedDir[100] = "/";
//   char newDir[100];
//   DIR* dir;
//   
//   // If navigate up
//   if (!strcmp(parsed,".."))
//   {
//       strcpy(newDir, shelly->currentdir);
//       int removeLen = 0;
//       for (int i = strlen(shelly->currentdir); i > 0; i--)
//       {
//         if (newDir[i] == '/')
//           break;
//         
//         removeLen++;
//       }
//       newDir[strlen(shelly->currentdir)-removeLen] = '\0';
//       dir = opendir(newDir);
//   }
//   // If navigate down
//   else {
//     strcpy(newDir, shelly->currentdir);
//     strcat(parsedDir, parsed);
//     strcat(newDir, parsedDir);
//     dir = opendir(newDir);
//   }
//   
//   
//   if (dir) {
//     // Directory Exists
//     dp = readdir(dir);
//     
//     strcpy(shelly->currentdir, newDir);
//     printf("    changed directory: %s\n", shelly->currentdir);
//     
//     closedir(dir);
//   } else if (ENOENT == errno) {
//     // Directory does not exist.
//     printf("  Does not exist");
//   } else {
//     // opendir() failed for some other reason.
//     printf("  What");
//   }
//
//   
//   return;
//     
// }

void cmdHistory(char* parsed, Shell* shelly)
{
  int i, j = 0;

    // Clears command history
    if (parsed != NULL && !strcmp(parsed, "-c"))
    {
        for (i = 0; i < shelly->cmdCnt; i++)
            strcpy(shelly->cmdHist[i], "");

        strcpy(shelly->cmdHist[0], "history -c");
        strcpy(parsed, "");
        shelly->cmdCnt = 1;

        return;
    }
    // Prints command history
    else
    {
        for (i = shelly->cmdCnt - 1; i != 0; i--)
            printf("[%d]:   %s\n", j++, shelly->cmdHist[i]);

        return;
    }
}

// char** replay(char* parsed, Shell* shelly)
// {
//   char** newParse;
//   
//   int histcnt = ((int) *(parsed) - 46); // Don't ask, no clue why it's needed
//   int parsedint = ((int) *(parsed) - 48);
//   printf("Replaying command [%d]:   %s\n", parsedint , shelly->cmdHist[shelly->cmdCnt - histcnt]);
//   
//   char *string;
//   char *found;
//   int i = 0;
//
//     string = strdup(shelly->cmdHist[shelly->cmdCnt - histcnt]);
//   
//   printf("%s\n", string);
//   newParse[0] = strsep(&string, " ");
//   printf("%s\n", string);
//   newParse[1] = strsep(&string, " ");
//   
//   fflush(stdout);
//   return newParse;
// }

// void byebye(Shell* shelly)
// {
//   printf("\nGoodbye\n");
//   char* filename = shelly->mainDir;
//   strcat(filename, "/hist");
//   FILE *file = fopen(filename, "w");
//   int results;
//   for (int i = shelly->cmdCnt - 1; i > 0; i--)
//   {
//             results = fputs(shelly->cmdHist[i], file);
//       results = fputs("\n",file);
//   }
//   fclose(file);
//   free(shelly);
//     exit(0);
// }

// Help command
void openHelp()
{
    printf("(1) 'movetodir [directory]'     Changes direcory. No need for '/' before folder name\n");
  printf("(2) 'whereami'        Prints current directory\n");
  printf("(3) 'history [-c]'        Prints history or clears history if '-c' is present\n");
  printf("(4) 'byebye'        Terminates Shell\n");
  printf("(5) 'replay [number]'       Re-executes the command labeled with number in the history\n");
  printf("(6) 'start program [parameters]' \n");
  printf("(7) 'background program [parameters]' \n");
  printf("(8) 'dalek PID' \n");
  
    return;
}
  
// Function to execute builtin commands
// int ownCmdHandler(char** parsed, Shell* shelly)
// {
// 	int NoOfOwnCmds = 7, i, switchOwnArg = 0;
// 	char* ListOfOwnCmds[NoOfOwnCmds];
// 	char* username;
//   char* cmdLine;
//   char** replayParse;
//   
// 	ListOfOwnCmds[0] = "byebye";
// 	ListOfOwnCmds[1] = "replay";
// 	ListOfOwnCmds[2] = "help";
// 	ListOfOwnCmds[3] = "hello";
//   ListOfOwnCmds[4] = "movetodir";
//   ListOfOwnCmds[5] = "whereami";
//   ListOfOwnCmds[6] = "history";
//   
// 	for (i = 0; i < NoOfOwnCmds; i++) {
// 		if (strcmp(parsed[0], ListOfOwnCmds[i]) == 0) {
// 			switchOwnArg = i + 1;
// 			break;
// 		}
// 	}
//   
// 	switch (switchOwnArg) {
// 	case 1:
// 		byebye(shelly);
// 	case 2:
// 		replayParse = replay(parsed[1], shelly);
// 	
// 	//printf("\n%s", replayParse[0]);
// 	//printf("\n%s", replayParse[1]);
// 	fflush(stdout);
// 	return 1;
// 			//return ownCmdHandler(replayParse, shelly);
// 	case 3:
// 		openHelp();
// 		return 1;
// 	case 4:
// 		username = getenv("USER");
// 		printf("\nHello %s.\nMind that this is "
// 				"not a place to play around."
// 				"\nUse help to know more..\n",
// 				username);
// 		return 1;
//   case 5:
//     if (parsed[1] != NULL)
//       movetodir(parsed[1], shelly);
//     else printf("Need path");
//     return 1;
//   case 6:
//     printf("  %s", shelly->currentdir);
//     return 1;
//   case 7:
//     cmdHistory(parsed[1], shelly);
//     return 1;
//     default:
//         break;
//     }
//   
//     return 0;
// }
  
// function for finding pipe
int parsePipe(char* str, char** strpiped)
{
    int i;
    for (i = 0; i < 2; i++) {
        strpiped[i] = strsep(&str, "|");
        if (strpiped[i] == NULL)
            break;
    }
  
    if (strpiped[1] == NULL)
        return 0; // returns zero if no pipe is found.
    else {
        return 1;
    }
}
  
// function for parsing command words
void parseSpace(char* str, char** parsed)
{
    int i;
  
    for (i = 0; i < MAXLIST; i++) {
        parsed[i] = strsep(&str, " ");
  
        if (parsed[i] == NULL)
            break;
        if (strlen(parsed[i]) == 0)
            i--;
    }
}
  
// int processString(char* str, char** parsed, char** parsedpipe, Shell* shelly)
// {
//     char* strpiped[2];
//     int piped = 0;
//   
//     piped = parsePipe(str, strpiped);
//   
//     if (piped) {
//         parseSpace(strpiped[0], parsed);
//         parseSpace(strpiped[1], parsedpipe);
//   
//     } else {
//   
//         parseSpace(str, parsed);
//     }
//   
//     if (ownCmdHandler(parsed, shelly))
//         return 0;
//     else
//         return 1 + piped;
// }

// Function to take input
int takeInput(char* str, Shell* shelly)
{
    char* buf;
  
    buf = readline("\n>>> ");
    if (strlen(buf) != 0) {
        strcpy(shelly->cmdHist[shelly->cmdCnt++], buf);
        strcpy(str, buf);
        return 0;
    } else {
        return 1;
    }
}

int movetodir(char **argv)
{
	return 0;	
}

int whereami(char **argv)
{
	
	return 0;	
}

int history(char **argv)
{
	
	return 0;	
}

int byebye(char **argv)
{
	
	return 0;	
}

int replay(char **argv)
{
	
	return 0;	
}

int start(char **argv)
{
	
	return 0;	
}

int background(char **argv)
{
	
	printf("This is background!\n");
	return 0;	
}

int dalek(char **argv)
{
	
	return 0;	
}

int main()
{
	Job job;
	Process proc;
	init_job(&job);
	init_process(&proc);
	job.first_process = &proc;
	printf("returned: %d\n", parse(&job, "background sh -c suck my balls"));
	proc.func(NULL);
}

// int main()
// {
//     char inputString[MAXCOM], *parsedArgs[MAXLIST];
//     char* parsedArgsPiped[MAXLIST];
//     int execFlag = 0;
//   
//   // Initializing struct
//   Shell *shelly;
//   shelly = calloc(1, sizeof(Shell));
//     getcwd(shelly->currentdir, sizeof(shelly->currentdir));
//   getcwd(shelly->mainDir, sizeof(shelly->mainDir));
//   shelly->cmdCnt = 0;
//   
//   // Starter function
//     init_shell(shelly);
//   
//   
//     while (1) {
//         // take input
//         if (takeInput(inputString, shelly))
//             continue;
//         // process
//         execFlag = processString(inputString,
//         parsedArgs, parsedArgsPiped, shelly);
//         // execflag returns zero if there is no command
//         // or it is a builtin command,
//         // 1 if it is a simple command
//         // 2 if it is including a pipe.
//   
//         // execute
//         if (execFlag == 1)
//             execArgs(parsedArgs);
//   
//         if (execFlag == 2)
//             execArgsPiped(parsedArgs, parsedArgsPiped);
//     }
//     return 0;
// }
