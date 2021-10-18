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
#define PATH_MAX 4096

// TODO List
//
// # movetodir directory  (DONE)
//		do not use chdir()
//
// # whereami (DONE)
//
// # history[-c] (DONE)
//
// # byebye (DONE)
//
// # replay number
//
// # start program [parameters]
//
// # background program [parameters]
//
// # dalek PID
//
// Extra credit...


typedef struct Shell
{
	char cmdHist[MAXLIST][1024];
	char currentdir[PATH_MAX]; // Current directory path
	char mainDir[PATH_MAX];
	
	int cmdCnt;
} Shell;

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
		while ((c=fgetc(file)) != EOF)
		{
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
	while (cnt!=-1)
	{
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
    } else if (pid == 0) {
        if (execvp(parsed[0], parsed) < 0) {
            printf("\nCould not execute command..");
        }
        exit(0);
    } else {
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
void movetodir(char* parsed, Shell* shelly)
{
	struct dirent *dp;
	char parsedDir[100] = "/";
	char newDir[100];
	DIR* dir;
	
	// If navigate up
	if (!strcmp(parsed,".."))
	{
			strcpy(newDir, shelly->currentdir);
			int removeLen = 0;
			for (int i = strlen(shelly->currentdir); i > 0; i--)
			{
				if (newDir[i] == '/')
					break;
				
				removeLen++;
			}
			newDir[strlen(shelly->currentdir)-removeLen] = '\0';
			dir = opendir(newDir);
	}
	// If navigate down
	else {
		strcpy(newDir, shelly->currentdir);
		strcat(parsedDir, parsed);
		strcat(newDir, parsedDir);
		dir = opendir(newDir);
	}
	
	
	if (dir) {
		// Directory Exists
		dp = readdir(dir);
		
		strcpy(shelly->currentdir, newDir);
		printf("    changed directory: %s\n", shelly->currentdir);
		
		closedir(dir);
	} else if (ENOENT == errno) {
		// Directory does not exist.
		printf("	Does not exist");
	} else {
		// opendir() failed for some other reason.
		printf("	What");
	}

	
	return;
		
}

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

void byebye(Shell* shelly)
{
	printf("\nGoodbye\n");
	char* filename = shelly->mainDir;
	strcat(filename, "/hist");
	FILE *file = fopen(filename, "w");
	int results;
	for (int i = shelly->cmdCnt - 1; i > 0; i--)
	{
            results = fputs(shelly->cmdHist[i], file);
			results = fputs("\n",file);
	}
	fclose(file);
	free(shelly);
    exit(0);
}

// Help command
void openHelp()
{
    puts("REEEEEEEEEEEEE");
  
    return;
}
  
// Function to execute builtin commands
int ownCmdHandler(char** parsed, Shell* shelly)
{
    int NoOfOwnCmds = 7, i, switchOwnArg = 0;
    char* ListOfOwnCmds[NoOfOwnCmds];
    char* username;
	char* cmdLine;
	
    ListOfOwnCmds[0] = "byebye";
    ListOfOwnCmds[1] = "cd";
    ListOfOwnCmds[2] = "help";
    ListOfOwnCmds[3] = "hello";
	ListOfOwnCmds[4] = "movetodir";
	ListOfOwnCmds[5] = "whereami";
	ListOfOwnCmds[6] = "history";
  
    for (i = 0; i < NoOfOwnCmds; i++) {
        if (strcmp(parsed[0], ListOfOwnCmds[i]) == 0) {
            switchOwnArg = i + 1;
            break;
        }
    }
  
    switch (switchOwnArg) {
    case 1:
        byebye(shelly);
    case 2:
        chdir(parsed[1]);
        return 1;
    case 3:
        openHelp();
        return 1;
    case 4:
        username = getenv("USER");
        printf("\nHello %s.\nMind that this is "
            "not a place to play around."
            "\nUse help to know more..\n",
            username);
        return 1;
	case 5:
		if (parsed[1] != NULL)
			movetodir(parsed[1], shelly);
		else printf("Need path");
		return 1;
	case 6:
		printf("	%s", shelly->currentdir);
		return 1;
	case 7:
		cmdHistory(parsed[1], shelly);
		return 1;
    default:
        break;
    }
  
    return 0;
}
  
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
  
int processString(char* str, char** parsed, char** parsedpipe, Shell* shelly)
{
    char* strpiped[2];
    int piped = 0;
  
    piped = parsePipe(str, strpiped);
  
    if (piped) {
        parseSpace(strpiped[0], parsed);
        parseSpace(strpiped[1], parsedpipe);
  
    } else {
  
        parseSpace(str, parsed);
    }
  
    if (ownCmdHandler(parsed, shelly))
        return 0;
    else
        return 1 + piped;
}

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

int main()
{
    char inputString[MAXCOM], *parsedArgs[MAXLIST];
    char* parsedArgsPiped[MAXLIST];
    int execFlag = 0;
	
	// Initializing struct
	Shell *shelly;
	shelly = calloc(1, sizeof(Shell));
    getcwd(shelly->currentdir, sizeof(shelly->currentdir));
	getcwd(shelly->mainDir, sizeof(shelly->mainDir));
	shelly->cmdCnt = 0;
	
	// Starter function
    init_shell(shelly);
	
	
    while (1) {
        // take input
        if (takeInput(inputString, shelly))
            continue;
        // process
        execFlag = processString(inputString,
        parsedArgs, parsedArgsPiped, shelly);
        // execflag returns zero if there is no command
        // or it is a builtin command,
        // 1 if it is a simple command
        // 2 if it is including a pipe.
  
        // execute
        if (execFlag == 1)
            execArgs(parsedArgs);
  
        if (execFlag == 2)
            execArgsPiped(parsedArgs, parsedArgsPiped);
    }
    return 0;
}