#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <dirent.h>
#include <errno.h>

#define MAXCOM 1000 // max number of letters to be supported
#define MAXLIST 100 // max number of commands to be supported
  
// Clearing the shell using escape sequences
#define clear() printf("\033[H\033[J")
#define HIST_FILEPATH "$HOME/.shelly-history"
#define DEFAULT_PROMPT "$PWD # "

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

struct Shell
{
	CmdHist *hist;
	int hist_len;
  char *hist_filepath;
  char *cwd; // Current directory path
  // char mainDir[ARG_MAX_LEN];
  
	int bgpids[MAX_BG_JOBS];
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
int parse(CmdFunc *func, char argv[ARG_MAX][ARG_MAX_LEN], int *argc, char *cmd);
void env_find_replace(char *dest, char *str);

void termination_handler(int signum);

int movetodir(Shell *shelly, CmdArgv argv, int argc);
int whereami(Shell *shelly, CmdArgv argv, int argc);
int history(Shell *shelly, CmdArgv argv, int argc);
void history_rev(CmdHist *hist, int i);
int byebye(Shell *shelly, CmdArgv argv, int argc);
int replay(Shell *shelly, CmdArgv argv, int argc);
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

Shell *root_shell = NULL;

static const CmdDef builtin_cmds[] = {
	{"movetodir", movetodir, movetodir_help},
	{"whereami", whereami, whereami_help},
	{"history", history, history_help},
	{"byebye", byebye, byebye_help},
	{"replay", replay, replay_help},
	{"start", start, start_help},
	{"background", background, background_help},
	{"dalek", dalek, dalek_help},
	{"exit", shell_exit, NULL},
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

CmdFunc parse_cmd(char *cmd_name)
{
	CmdDef bi_cmd;		
	int i = 0;

	while ((bi_cmd = builtin_cmds[i]).cmd_name != NULL) {
		if (strcmp(bi_cmd.cmd_name, cmd_name) == 0)
			return bi_cmd.func;

		i++;
	}
	
	return NULL;
}

// cmd is a null or \n terminated string
int parse(CmdFunc *func, char argv[ARG_MAX][ARG_MAX_LEN], int *argc, char *cmd)
{
	int arg = 0;
	int arg_len = 0;
	int parsing_arg = 0;
	int reached_end = 0;
	char c;
	int i;
	
  // This is horrible, I know. Breaks when reached_end is true
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
					CmdFunc cmd_func = parse_cmd(argv[0]);
					if (cmd_func == NULL)
						return PARSE_INVALID_CMD;

					*func = cmd_func;
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

void env_find_replace(char *dest, char *str)
{
	int front = 0;
	int back = 0;
	char key[ARG_MAX_LEN];
	char *val;
	char c = str[0];
	int in_key = 0;

	while (c != '\0' && back < ARG_MAX_LEN) {
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

	if (is_interactive) {
		root_shell = shelly;
		signal(SIGTERM, termination_handler);
		signal(SIGINT, SIG_IGN);
	}

	char *prompt = getenv("PROMP");
	if (prompt) {
		shelly->prompt = prompt;
	}
	else {
		shelly->prompt = DEFAULT_PROMPT;
	}
}

void exit_shell(Shell *shelly)
{
  free(shelly->cwd);
  free(shelly->hist_filepath);

	CmdHist *hist = shelly->hist, *temp;
	while (hist != NULL) {
		temp = hist->next;
		free(hist);
		hist = temp;
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
			cmd[i + 1] = '\0';
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
  FILE *hist_file = fopen(shelly->hist_filepath, "a");
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

	env_find_replace(buf_env, shelly->prompt);
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

int start(Shell *shell, CmdArgv argv, int argc)
{
  printf("start\n");	
	return 0;
}

int start_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("start <program> [param]      start a program\n");	
	return 0;
}


int background(Shell *shell, CmdArgv argv, int argc)
{
  printf("background\n");	
	return 0;
}

int background_help(Shell *shell, CmdArgv argv, int argc)
{
  printf("background <program> [param] start a program in the background\n");	
	return 0;
}


int dalek(Shell *shell, CmdArgv argv, int argc)
{
  printf("dalek\n");	
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
	while (*path != '/') {
		*dir = *path;

		if (*path == '\0')
			return NULL;

		path++;
		dir++;
	}

	// Move until next non-'/' to supports paths like
	// /dev///whoops/too//many//slashes
	while (*path == '/')
		path++;

	return path;
}

int movetodir(Shell *shell, CmdArgv argv, int argc)
{
	if (argc != 2)
		return 1;
	
	char *path = argv[1];
	char dirname[CMD_MAX_LEN];

	if (chdir(path) != 0) {
		printf("The directory '%s' does not exist!\n", path);
	}
	else {
		char cwd[PATH_MAX];
		getcwd(cwd, sizeof(cwd));
		setenv("PWD", cwd, 1);
		printf("Changed directory to '%s'\n", cwd);
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
	history_rev(shell->hist, 0);

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
	int i = replay_num;
	CmdHist *hist = shell->hist;
	CmdArgv replay_argv;
	int replay_argc;
	CmdFunc replay_func;
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
	int parse_result = parse(&replay_func, replay_argv, &replay_argc, hist->cmd);	
	if (!parse_result) {
		if (replay_func == replay) 
			recursive_relay_count++;

		replay_func(shell, replay_argv, replay_argc);	

		if (replay_func == replay) 
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

void termination_handler(int signum)
{
	if (root_shell == NULL)
		return;

	exit_shell(root_shell);	
}

int main()
{
	Shell shelly;
  char cmd_buf[CMD_MAX_LEN];
  // This will be dynamically allocated later...maybe
  char cmd_argv[ARG_MAX][ARG_MAX_LEN];
	int cmd_argc = 0;
  CmdFunc cmd;

	init_shell(&shelly, 1);
	printf("%s\n", get_random_greeting());
  // print_hist_list(&shelly);
  
  while (shelly.is_running) {
    if (take_input(&shelly, cmd_buf)) {
      // printf("\n");
    }
    else {
      enum ParseStatus status = parse(&cmd, cmd_argv, &cmd_argc, cmd_buf);
      switch(status) {
        case PARSE_OK:
          cmd(&shelly, cmd_argv, cmd_argc);
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
