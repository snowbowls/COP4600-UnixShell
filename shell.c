/*
*
*	Assignment 2
*
* MyShell (or Shelly)
* 
* Authors: Paul Wood
*		   James Henderson
*
* BUILD INSTRUCTIONS:
*		gcc -lreadline -pthread -lpthread -o shelly shell.c
*
* */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <threads.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <dirent.h>
#include <errno.h>

#define MAXCOM 1000 // max number of letters to be supported
#define MAXLIST 100 // max number of commands to be supported
  
// Clearing the shell using escape sequences
#define clear() printf("\033[H\033[J")
#define HIST_FILEPATH "$HOME/.shelly-history"
#define DEFAULT_PROMPT "$PWD# "
#define ENV_PROMPT "SHELLY_PROMPT"
#define REPL_ENV_CHAR '{'
#define REPL_WS_CHAR '\\'
// Using this for getcwd
#define ARG_MAX_LEN 1024
#define ARG_MAX 64
#define CMD_MAX_LEN (ARG_MAX_LEN * ARG_MAX)
#define MAX_BG_JOBS 64
#define NUM_BUILTIN_CMDS 8

#define IS_WHITESPACE(c) (c == ' ' || c == '\t' || c == '\r' || c == '\n')
// Any control, operator, or pipeline char is treated as an arg string
#define IS_ALLOWED(c) (c > 32 && c < 127)
// #define IS_CONTROL(c) ()
// #define IS_PIPELINE(c) ()
// #define IS_OPERATOR(c) ()
// #define IS_UNSUPPORTED(c) ()

typedef struct Shell Shell;

// This really should just be a linked-list... smh
typedef char CmdArgv[ARG_MAX][ARG_MAX_LEN];
typedef int (*CmdFunc)(Shell*, CmdArgv, int);

typedef struct CmdHist CmdHist;
struct CmdHist {
	CmdHist *next;
	char cmd[CMD_MAX_LEN];
  // Opted to re-parse for memory saving and simplicity
  // CmdFunc func;
  // CmdArgv vargs;
};

typedef struct IntList IntList;
struct IntList {
	int data;
	IntList *next;
};

struct Shell
{
	CmdHist *hist;
	int hist_len;
  char *hist_filepath;
  char *cwd; // Current directory path
  // char mainDir[ARG_MAX_LEN];
  
  int infile, outfile, errfile;
	IntList *bgpids;
	mtx_t bg_mtx;
	int num_bgpids;
	int is_running;

	char *prompt;
};

typedef struct CmdDef {
	char *cmd_name;
	CmdFunc func;
	CmdFunc help;
} CmdDef;


enum ParseStatus {PARSE_OK=0, PARSE_INVALID_CHAR=1, PARSE_INVALID_CMD=2};

void init_shell(Shell*, int);
void exit_shell(Shell *shelly);
void read_hist_file(Shell *shelly, FILE *hist_file);
CmdHist* create_cmd_hist(char *cmd);
void add_to_hist(Shell *shelly, char *buf);
int parse(const CmdDef **cmd_def, CmdArgv argv, int *argc, char *cmd);
void env_find_replace(char *dest, char *str);
void print_hist_list(Shell *shelly);
void free_hist_ll(Shell *shelly);

void termination_handler(int signum);
void child_term_handler(int signum);
void add_bgpid(Shell *shelly, int pid);
void remove_bgpid(Shell *shelly, int pid);
void kill_child(int pid);

int print_bgpids(Shell *shelly, CmdArgv argv, int argc);
int print_bgpids_help(Shell *shelly, CmdArgv argv, int argc);

int dalekall(Shell *shell, CmdArgv argv, int argc);
int movetodir(Shell *shelly, CmdArgv argv, int argc);
int whereami(Shell *shelly, CmdArgv argv, int argc);
int set_env(Shell *shell, CmdArgv argv, int argc);
int history(Shell *shelly, CmdArgv argv, int argc);
void history_rev(CmdHist *hist, int i);
int byebye(Shell *shelly, CmdArgv argv, int argc);
int replay(Shell *shelly, CmdArgv argv, int argc);
int repeat(Shell *shelly, CmdArgv argv, int argc);
int start(Shell *shelly, CmdArgv argv, int argc);
int background(Shell *shelly, CmdArgv argv, int argc);
int dalek(Shell *shelly, CmdArgv argv, int argc);
int movetodir_help(Shell *shelly, CmdArgv argv, int argc);
int whereami_help(Shell *shelly, CmdArgv argv, int argc);
int history_help(Shell *shelly, CmdArgv argv, int argc);
int byebye_help(Shell *shelly, CmdArgv argv, int argc);
int replay_help(Shell *shelly, CmdArgv argv, int argc);
int start_help(Shell *shelly, CmdArgv argv, int argc);
int background_help(Shell *shelly, CmdArgv argv, int argc);
int dalek_help(Shell *shelly, CmdArgv argv, int argc);
int shell_exit(Shell *shell, CmdArgv argv, int argc);
int shell_help(Shell *shell, CmdArgv argv, int argc);
int set_env_help(Shell *shell, CmdArgv argv, int argc);
int repeat_help(Shell *shelly, CmdArgv argv, int argc);
int dalekall_help(Shell *shell, CmdArgv argv, int argc);

Shell *root_shell = NULL;
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;


// Literally just realized that the help function can just be a string... GUH
const CmdDef builtin_cmds[] = {
	{"movetodir", movetodir, movetodir_help},
	{"whereami", whereami, whereami_help},
	{"history", history, history_help},
	{"byebye", byebye, byebye_help},
	{"replay", replay, replay_help},
	{"start", start, start_help},
	{"background", background, background_help},
	{"repeat", repeat, repeat_help},
	{"dalek", dalek, dalek_help},
	{"dalekall", dalekall, dalekall_help},
	{"kill", dalek, NULL},
	{"killall", dalekall, NULL},
    {"set", set_env, set_env_help},
	{"exit", shell_exit, NULL},
	{"lsbg", print_bgpids, print_bgpids_help},
	{"help", shell_help, NULL},
	{NULL, NULL, NULL}
};

static const char *greetings[] = {
	"The shell to end all shells\n"
	"... I hope I don't break anything...\n"

	"       .----.   @   @       \n"
  "      / .-\"-.`.  \\v/      \n"
  "      | | '\\ \\ \\_/ )     \n"
  "    ,-\\ `-.' /.'  /        \n"
  "    '---`----'----'         \n"
	,
	
	NULL
};

const CmdDef* parse_cmd(char *cmd_name)
{
	CmdDef bi_cmd;		
	int i = 0;

	while ((bi_cmd = builtin_cmds[i]).cmd_name != NULL) {
		if (strcmp(bi_cmd.cmd_name, cmd_name) == 0)
			return builtin_cmds + i;

		i++;
	}
	
	return NULL;
}

// cmd is a null or \n terminated string
int parse(const CmdDef **cmd_def, CmdArgv argv, int *argc, char *cmd)
{
	int arg = 0;
	int arg_len = 0;
	int parsing_arg = 0;
	int reached_end = 0;
	char c;
	int i;
	
	for (i = 0; i < ARG_MAX_LEN; i++) {
		c = cmd[i];

		if (c == '\0' || c == '\n')	
			reached_end = 1;	/* Need to finish parsing arg */
		
		if (IS_WHITESPACE(c) || reached_end) {
			// Done parsing arg
			if (parsing_arg) {
				argv[arg][arg_len] = '\0';
				// printf("parsed argv: '%s'\n", argv[arg]);

				if (arg == 0) {
					const CmdDef* cd = parse_cmd(argv[0]);
					if (cd == NULL)
						return PARSE_INVALID_CMD;

					*cmd_def = cd;
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
			if (c == REPL_ENV_CHAR)
				c = '$';
			else if (c == REPL_WS_CHAR)
				c = ' ';
			argv[arg][arg_len] = c;
			arg_len++;
		}
		else {
			return PARSE_INVALID_CHAR;
		}
	}

	*argc = arg;

	return PARSE_OK;
}

void source_file(Shell *shelly, char *filepath)
{
	
}



int launch_process(char **argv, int argc, 
  int pgid, int infile, int outfile, int errfile, int foreground)
{

  // Forking a child
  pid_t pid = fork(); 

  if (pid == -1) {
    printf("\nFailed forking child..");
    return -1;
  } else if (pid == 0) {
    pid = getpid ();
    if (pgid == 0) pgid = pid;
      setpgid (pid, pgid);

    if (infile != STDIN_FILENO) {
      dup2(infile, STDIN_FILENO);
      close(infile);
    }
    if (outfile != STDOUT_FILENO) {
      dup2(outfile, STDOUT_FILENO);
      close(outfile);
    }
    if (infile != STDERR_FILENO) {
      dup2(errfile, STDERR_FILENO);
      close(errfile);
    }

    if (execvp(argv[0], argv) < 0) {
      switch (errno) {
        case EACCES:
          printf("Access denied.\n");
          break;
        case EIO:
          printf("An I/O error has occured.\n");
          break;
        case ENOENT:
          printf("Does not exist.\n");
          break;
        default:
          printf("An error has occured (%d)\n", errno);
          break;
      }
    }
    exit(1);
  } else {
    // waiting for child to terminate
		if (foreground) {
			wait(NULL); 
			return 0;
		}
		else {
			return pid;
		}
  }
}

void env_find_replace(char *dest, char *str)
{
	int front = 0;
	int back = 0;
	char key[ARG_MAX_LEN];
	char *val;
	char c = str[0];
	int in_key = 0;

	// Need to check if in_key=true and c=='\0' to cover the edge case where the
	// env var is at the end of the string.
	while ((c != '\0' || in_key) && back < ARG_MAX_LEN) {
		if (c == '$')	{
			in_key = 1;
			back++;
			front++;
			c = str[front];
		}
		else if (in_key) {
			if (isalpha(c) || c == '_') {
				front++;
				c = str[front];
			}
			else {
				// check env
				memcpy((void*) key, (void*) (str + back), front - back);
				key[front - back] = '\0';
				// printf("key:'%s', strlen=%d\n", key, strlen(key));
				val = getenv(key);
				// printf("val:'%s'\n", val);
				if (val) {
					while (*val != '\0') {
						*dest = *val;
						val++;
						dest++;
					}
					back = front;
				}
				else {
					for (; back <= front; back++) {
						*dest = str[back];
					}
				}

				in_key = 0;
			}
		}
		else {
			*dest = c;
			dest++;
			front++;
			back++;
			c = str[front];
		}

		// printf("front=%d, back=%d\n", front, back);
	}

	*dest = '\0';
}


const char* get_random_greeting() {
	int len = 0;	

	while (greetings[len] != NULL)
		len++;
	
	return greetings[rand() % len];	
}

void init_shell(Shell *shelly, int is_interactive) {
	char *hist_filepath = (char *) malloc(sizeof(char) * ARG_MAX_LEN);
	FILE *hist_file;
	shelly->hist_len = 0;

	env_find_replace(hist_filepath, HIST_FILEPATH);
  shelly->hist_filepath = hist_filepath;
  shelly->hist = NULL;
	// printf("%s\n", hist_filepath);
  hist_file = fopen(hist_filepath, "r");
	if (hist_file) {
		read_hist_file(shelly, hist_file);
    fclose(hist_file);
	}

	shelly->cwd = getcwd(NULL, 0);
	shelly->is_running = 1;
	shelly->bgpids = NULL;
	shelly->num_bgpids = 0;
	if (mtx_init(&(shelly->bg_mtx), mtx_plain) != thrd_success) {
		printf("Unable to create mutex for bg job list!\n");
		exit(1);
	}

	// if (is_interactive) {
	// 	root_shell = shelly;
	// 	signal(SIGTERM, termination_handler);
	// 	signal(SIGINT, SIG_IGN);
	// }

  /* See if we are running interactively.  */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty (shell_terminal);

  if (shell_is_interactive) {
		root_shell = shelly;
    /* Loop until we are in the foreground.  */
    while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
      kill (- shell_pgid, SIGTTIN);

    /* Ignore interactive and job-control signals.  */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, child_term_handler);

    /* Put ourselves in our own process group.  */
    shell_pgid = getpid ();
    if (setpgid (shell_pgid, shell_pgid) < 0)
      {
        perror ("Couldn't put the shell in its own process group");
        exit (1);
      }

    /* Grab control of the terminal.  */
    tcsetpgrp (shell_terminal, shell_pgid);

    /* Save default terminal attributes for shell.  */
    tcgetattr (shell_terminal, &shell_tmodes);
  }

  shelly->infile = STDIN_FILENO;
  shelly->outfile = STDOUT_FILENO;
  shelly->errfile = STDERR_FILENO;

	char *prompt = getenv(ENV_PROMPT);
	if (!prompt) {
		setenv(ENV_PROMPT, DEFAULT_PROMPT, 1);
	}
}

void free_hist_ll(Shell *shelly)
{
	CmdHist *hist = shelly->hist, *temp;
	while (hist != NULL) {
		temp = hist->next;
		free(hist);
		hist = temp;
	}

	shelly->hist = NULL;
}

void exit_shell(Shell *shelly)
{
  free(shelly->cwd);
  free(shelly->hist_filepath);

	free_hist_ll(shelly);

	IntList *bgpid = shelly->bgpids, *temp_bgpid;

	// Just let the children finish I guess 
	while (bgpid != NULL) {
		temp_bgpid = bgpid->next;
		free(bgpid);
		bgpid = temp_bgpid;
	}
}
  
  
// Must be null terminated
CmdHist* create_cmd_hist(char *cmd)
{
	CmdHist *hist = (CmdHist *)	malloc(sizeof(CmdHist));
	hist->next = NULL;
	strcpy(hist->cmd, cmd);

	return hist;
}

void read_hist_file(Shell *shelly, FILE *hist_file)
{
	char c;
	int i = 0;
	char cmd[CMD_MAX_LEN];
	CmdHist *read_hist;

  // printf("shelly hist:%p\n", shelly->hist);
	while ((c=fgetc(hist_file)) != EOF) {
		//shelly->cmdHist[1] = c;
		if (c != '\n' && c != '\r')
			cmd[i++] = c;
		else if (i > 0) {
			cmd[i] = '\0';
      // printf("'%s'\n", cmd);
			read_hist = create_cmd_hist(cmd);
			read_hist->next = shelly->hist;
			shelly->hist = read_hist;
			shelly->hist_len++;
			i = 0;
		}
	}
}

void add_to_hist(Shell *shelly, char *buf)
{
	// Add to shelly hist list
	CmdHist *hist = create_cmd_hist(buf);
	hist->next = shelly->hist;
	shelly->hist = hist;
	shelly->hist_len++;

	// Write to hist file
  FILE *hist_file = fopen(shelly->hist_filepath, "a+");
	if (hist_file) {
		fprintf(hist_file, "%s\n", buf);
    fclose(hist_file);
  }
}

// TODO: if we have time
// Idea is to create a TabCompl struct that contains ComplSrc structs and a
// ComplContext struct. The ComplSrc provide the autocomplete options while the
// ComplContext struct contains the context (like compl for command, directory,
// etc).
int tab_complete(char **results, char *buf)
{
	return 0;	
}


// Function to take input
int take_input(Shell* shelly, char* str)
{
	char c;
	int i = 0;
	char buf[CMD_MAX_LEN];
	char buf_env[CMD_MAX_LEN];

	// printf("ti: '%s'\n", getenv(ENV_PROMPT));
	// printf("pwd: '%s'\n", getenv("PWD"));
	env_find_replace(buf_env, getenv(ENV_PROMPT));
	printf("%s", buf_env);
	while ((c = getchar()) != '\n') {
		if (c == '\t') {
	
		}	
		else {
			buf[i] = c;
			i++;
		}

		if (i >= CMD_MAX_LEN - 1) {
			printf("Command too long!\n");
			return 1;
		}
	}

	buf[i] = '\0';
	
	if (strlen(buf) == 0)
		return 1;

	env_find_replace(buf_env, buf);
  add_to_hist(shelly, buf);
	strcpy(str, buf_env);
	return 0;

	// buf = readline("\n>>> ");
	// if (strlen(buf) != 0) {
	// 	add_to_hist(shelly, buf);
	// 	env_find_replace(buf_env, buf);
	// 	strcpy(str, buf_env);
	//
	// 	return 0;
	// }
	// else {
	// 	return 1;
	// }
}

int repeat(Shell *shell, CmdArgv argv, int argc)
{
	if (argc < 3)
		return 1;

	int repeat_count = strtol(argv[1], NULL, 10);

  char **process_args = (char **) malloc(sizeof(char *) * (argc + 1)); 
  int i;
  for (i = 0; i < argc - 1; i++) {
    process_args[i] = argv[i + 2];
  }
  process_args[i] = NULL;

	for (int i = 0; i < repeat_count; i++) {
		printf(
		  "pid: %d\n",
			launch_process(
				process_args, argc, shell_pgid, shell->infile, 
				shell->outfile, shell->errfile, 0
			)
		);
	}

	free(process_args);
	return 0;
}

int repeat_help(Shell *shell, CmdArgv argv, int argc)
{
	printf("repeat <n> <command>         repeat <command> <n> times\n");
	return 0;
}

int start(Shell *shell, CmdArgv argv, int argc)
{
  char **process_args = (char **) malloc(sizeof(char *) * (argc + 1)); 
  int i;
  for (i = 0; i < argc - 1; i++) {
    process_args[i] = argv[i + 1];
  }
  process_args[i] = NULL;

  launch_process(
		process_args, argc, shell_pgid, 
		shell->infile, shell->outfile, shell->errfile, 1
	);
	free(process_args);

	return 0;
}

int start_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("start <program> [param]      start a program\n");	
	return 0;
}


int background(Shell *shell, CmdArgv argv, int argc)
{
  int dev_null = open("/dev/null", O_WRONLY);
	int pid;

  char **process_args = (char **) malloc(sizeof(char *) * (argc + 1)); 
  int i;
  for (i = 0; i < argc - 1; i++) {
    process_args[i] = argv[i + 1];
  }
  process_args[i] = NULL;

 if (mtx_lock(&shell->bg_mtx) != thrd_success) {
		printf("Unable to get lock on bg job list!\n");
		exit(1);
	}

	pid = launch_process(
		process_args, argc, shell_pgid, -1, dev_null, dev_null, 0
	);

	add_bgpid(shell, pid);
	mtx_unlock(&shell->bg_mtx);

  printf("printing to /dev/null :)\n%d\n", pid);

	free(process_args);
	return 0;
}

int background_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("background <program> [param] start a program in the background\n");	
	return 0;
}

void kill_child(int pid)
{
  // Check if the pid exists
  kill(pid, 0);
  if (errno == ESRCH) {
    printf("The process with pid %d does not exist.\n", pid);
  }
  else {
    // No mercy
    kill(pid, SIGKILL);
    if (errno == EPERM)
      printf("Permission denied.\n");
    else
      printf("Killed %d\n", pid);
  }
}


int dalekall(Shell *shell, CmdArgv argv, int argc)
{
	IntList *cur = shell->bgpids, *temp;
	mtx_lock(&shell->bg_mtx);
	while (cur != NULL) {
		kill_child(cur->data);
		temp = cur;
		cur = cur->next;
		free(temp);
	}

	mtx_unlock(&shell->bg_mtx);
	shell->bgpids = NULL;

	return 0;
}

int dalekall_help(Shell *shell, CmdArgv argv, int argc)
{
	printf("dalekall                     execute order 66\n");
	return 0;
}

int dalek(Shell *shell, CmdArgv argv, int argc)
{
  if (argc != 2)
    return 1;

  int pid = strtol(argv[1], NULL, 10);
	kill_child(pid);

	return 0;
}

int dalek_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("dalek <pid>                  kill the process w/ pid <pid>\n");	
	return 0;
}

// Write alpha numberic dirname to dir and returns the ending location in path
// Returns NULL at the end of the path
char* get_next_dir(char *dir, char *path)
{
  if (*path == '/') {
    dir[0] = '/';
    dir[1] = '\0'; 
    return path + 1;
  }

	while (*path != '/') {
    putc(*path, stdin);
		*dir = *path;

		if (*path == '\0')
			return NULL;

		path++;
		dir++;
	}

  *dir = '\0';

	// Move until next non-'/' to supports paths like
	// /dev///whoops/too//many//slashes
	while (*path == '/')
		path++;

	return path;
}

int movetodir(Shell *shelly, CmdArgv argv, int argc)
{
	if (argc != 2)
		return 1;
	
	char *path = argv[1];
	char dirname[CMD_MAX_LEN];
  
  // while ((path = get_next_dir(dirname, path)) != NULL)
  //   printf("%s\n", dirname);
  // printf("%s\n", dirname);

  struct dirent *dp;
	char parsedDir[100] = "/";
	char newDir[100];
	DIR* dir;
	
	// If navigate up
	if (!strcmp(path,".."))
	{
    strcpy(newDir, shelly->cwd);
    int removeLen = 0;
    for (int i = strlen(shelly->cwd); i > 0; i--)
    {
      if (newDir[i] == '/')
        break;
      
      removeLen++;
    }
    newDir[strlen(shelly->cwd)-removeLen] = '\0';
    dir = opendir(newDir);
	}
	// If navigate down
	else {
		strcpy(newDir, shelly->cwd);
		strcat(parsedDir, path);
		strcat(newDir, parsedDir);
		dir = opendir(newDir);
	}
	
	
	if (dir) {
		// Directory Exists
		dp = readdir(dir);
    fchdir(dirfd(dir));
		
		strcpy(shelly->cwd, newDir);
    setenv("PWD", shelly->cwd, 1);
		printf("    changed directory: %s\n", shelly->cwd);
		
		closedir(dir);
	} else if (ENOENT == errno) {
		// Directory does not exist.
		printf("	Does not exist\n");
	} else {
		// opendir() failed for some other reason.
		printf("	What\n");
	}

	
	return 0;	
}

int movetodir_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("movetodir <dir>              change cwd\n");	
	return 0;	
}

int whereami(Shell *shell, CmdArgv argv, int argc)
{
	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));
	printf("%s\n", cwd);
	return 0;	
}

int whereami_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("whereami                     prints cwd\n");	
	return 0;	
}

int history(Shell *shell, CmdArgv argv, int argc)
{
	if (argc > 2)
		return 1;
	else if (argc == 2) {
		if (strcmp(argv[1], "-c") != 0)	
			return 1;
		else {
			remove(shell->hist_filepath);	
			free_hist_ll(shell);
		}
	}
	else {
		void print_hist_list(Shell *shelly);
		history_rev(shell->hist, 0);
	}

	return 0;	
}

void history_rev(CmdHist *hist, int i)
{
	if (hist->next)
		history_rev(hist->next, i+1);	
	
	printf("%d: %s\n", i, hist->cmd);
}

int history_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("history [-c]                 prints history\n"
				 "                             -c to clear history\n");	
	return 0;	
}

int byebye(Shell *shell, CmdArgv argv, int argc)
{
	shell_exit(shell, NULL, 0);
	return 0;	
}

int byebye_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("byebye                       exit shell - also can use 'exit'\n");	
	return 0;	
}

int replay(Shell *shell, CmdArgv argv, int argc)
{
	if (argc != 2) {
		replay_help(shell, argv, argc);
		return 1;
	}

	int replay_num = strtol(argv[1], NULL, 10);
  // Need to add one because the cmd string that called this function is now in
  // history
	int i = replay_num + 1;
	CmdHist *hist = shell->hist;
	CmdArgv replay_argv;
	int replay_argc;
	const CmdDef *replay_cmd;
	const int max_recursive_replay = 16;
	static int recursive_relay_count = 0;

	if (recursive_relay_count > max_recursive_replay) {
		printf("Hit maximum replay recursion count (%d)!\n", max_recursive_replay);
		return 1;
	}

	while (i > 0 && hist) {
		hist = hist->next;
		i--;
	}

	// Either couldn't go back replay_num spaces or we could but it's NULL
	if (i != 0 || hist == NULL) {
		printf("The history doesn't go back that far (%d)\n", replay_num);
		return 1;	
	}

	printf("Running '%s'\n", hist->cmd);
	int parse_result = parse(&replay_cmd, replay_argv, &replay_argc, hist->cmd);	
	if (!parse_result) {
		if (replay_cmd->func == replay) 
			recursive_relay_count++;

		replay_cmd->func(shell, replay_argv, replay_argc);	

		if (replay_cmd->func == replay) 
			recursive_relay_count--;
	}
	else {
		printf("Invalid command!\n");
	}

	return 0;	
}

int replay_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("replay <n>                   re-run the last n-th program\n");	
	return 0;	
}

int shell_exit(Shell *shell, CmdArgv argv, int argc)
{
	shell->is_running = 0;
	return 0;
}

int shell_help(Shell *shell, CmdArgv argv, int argc)
{
	printf("Mysh usage:\n");
	for (int i = 0; builtin_cmds[i].cmd_name != NULL; i++) {
		if (builtin_cmds[i].help)
			builtin_cmds[i].help(shell, NULL, 0);	
	}
	return 0;	
}

void print_hist_list(Shell *shelly)
{
  CmdHist *hist = shelly->hist;
  printf("Printing history list...\n");
  while (hist != NULL) {
    printf("%s\n", hist->cmd);
    hist = hist->next;
  }
  printf("End of history.\n");
}

int print_bgpids_help(Shell *shelly, CmdArgv argv, int argc)
{
	printf("lsbg                         print current background pids\n");
	return 0;
}

int print_bgpids(Shell *shelly, CmdArgv argv, int argc)
{
	IntList *cur = shelly->bgpids;
	while (cur != NULL) {
		printf("%d\n", cur->data);
		cur = cur->next;
	}

	return 0;
}

void add_bgpid(Shell *shelly, int pid)
{
	IntList *old = shelly->bgpids;
	IntList *new = (IntList *) malloc(sizeof(IntList));
	new->next = old;
	new->data = pid;
	shelly->bgpids = new;
}

void remove_bgpid(Shell *shelly, int pid)
{
	IntList *temp, *prev = NULL;
	temp = shelly->bgpids;

	while (temp != NULL && temp->data != pid) {
		prev = temp;
		temp = temp->next;
	}

	// Will not be NULL if stopped before end of the list
	if (temp != NULL) {
		if (prev == NULL) {
			// Is the head
			temp = temp->next;
			free(shelly->bgpids);
			shelly->bgpids = temp;
		}	
		else {
			prev->next = temp->next;
			free(temp);
		}
	}
}

void child_term_handler(int signum)
{
	// https://www.gnu.org/software/libc/manual/html_mono/libc.html#Process-Completion
	int pid, status, serrno;
  serrno = errno;
	
	mtx_lock(&root_shell->bg_mtx);

  while (1) {
		pid = waitpid (WAIT_MYPGRP, &status, WNOHANG);
		if (pid < 0)
			{
				// perror ("waitpid");
				break;
			}
		if (pid == 0)
			break;
		
		remove_bgpid(root_shell, pid);
		printf("\n    %d done\n", pid);
	}
	mtx_unlock(&root_shell->bg_mtx);
  errno = serrno;
}

void termination_handler(int signum)
{
	if (root_shell == NULL)
		return;

	exit_shell(root_shell);	
}

int set_env(Shell *shell, CmdArgv argv, int argc)
{
  if (argc > 3)
    return -1;

  char *key = argv[1];
  char *val;
  if (argc == 2)
    val = "";
  else
    val = argv[2];

  // printf("key=%s, val=%s\n", key, val);
  setenv(key, val, 1);
	// printf("val is now: '%s'\n", getenv(key));

  return 0;
}

int set_env_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("set <key> <value>            sets environment variable\n");	
  return 0;
}

int main()
{
	Shell shelly;
  char cmd_buf[CMD_MAX_LEN];
  // This will be dynamically allocated later...maybe
  char cmd_argv[ARG_MAX][ARG_MAX_LEN];
	int cmd_argc = 0;
  const CmdDef *cmd_def;

	init_shell(&shelly, 1);
	printf("%s\n", get_random_greeting());
  // print_hist_list(&shelly);
  
  while (shelly.is_running) {
    if (take_input(&shelly, cmd_buf)) {
      // printf("\n");
    }
    else {
      enum ParseStatus status = parse(&cmd_def, cmd_argv, &cmd_argc, cmd_buf);
			// printf("parse status: %d, cmd_def: %p\n", status, cmd_def);
      switch(status) {
        case PARSE_OK:
          if(cmd_def->func(&shelly, cmd_argv, cmd_argc) != 0) {
            printf("Usage:\n");
            cmd_def->help(&shelly, cmd_argv, cmd_argc);
          }

          break;
        case PARSE_INVALID_CHAR:
        case PARSE_INVALID_CMD:
          printf("Invalid command!\n");
          break;
      }
    }
  }

	exit_shell(&shelly);
}
