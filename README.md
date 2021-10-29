# COP4600-UnixShell
Assignment 2: MyShell (or as we call it, Shelly).
       .----.   @   @       
      / .-"-.`.  \v/      
      | | '\ \ \_/ )     
    ,-\ `-.' /.'  /        
    '---`----'----'

**TOC:**
1. Usage
2. Additional features
3. Additional (non-extra credit) commands
4. Extra credit commands


## Usage
```sh
	Mysh usage:
	movetodir <dir>              change cwd
	whereami                     prints cwd
	history [-c]                 prints history
															 -c to clear history
	byebye                       exit shell - also can use 'exit'
	replay <n>                   re-run the last n-th program
	start <program> [param]      start a program
	background <program> [param] start a program in the background
	repeat <n> <command>         repeat <command> <n> times
	dalek <pid>                  kill the process w/ pid <pid>
	dalekall                     execute order 66
	set <key> <value>            sets environment variable
	lsbg                         print current background pids
```

## To make
```sh
	make
```
Or to run immediately after building:
```sh
	make run
```

## Additional features
### Environment variable expansion
You can use environment variables in the commands that you run, like:
```
	start cat $HOME/a_file.txt
```

### Piping to a file
You can pipe the output for a foreground or background process, like so:
```sh
	start tree / > /dev/null
```
Or,
```sh
	background tree / > $HOME/tree.txt
```

### Keeps track of background commands
Every time a background command is started, it is added to a linked-list of
other currently running background commands. The list is guarded by a mutex.
Elements in the list are removed by the `child_term_handler()` function, which
properly locks and unlocks the mutex (like the launch background process
functions). 


### Can use environment variables in the prompt
Envrionment variables can be used in the prompt by setting the `SHELLY_PROMPT`
env variable. To avoid a lookup into the environment, `$` should be replaced
with `{`. Spaces can be added using the back-tick character (`). 

## Additional (non-extra credit) commands
### lsbg
Lists the currently running background processes.

### set <key> <value>
Sets an environment variable `<key>` with value `<value>`.

## Extra credit commands
### repeat
The following will open 5 instances of the [Kitty terminal](https://sw.kovidgoyal.net/kitty/)
```sh
	repeat 5 kitty
```

The following will perform ls 5 times in the background:
```sh
	repeat 5 ls	
```

Output:
```
	/home/paulw/repos/COP4600-UnixShell# repeat 5 ls
	pid: 12315
	pid: 12316
	pid: 12317
	pid: 12318
	pid: 12319
	/home/paulw/repos/COP4600-UnixShell# Makefile  README.md  shell.c  shelly  tree
	Makefile  README.md  shell.c  shelly  tree
	Makefile  README.md  shell.c  shelly  tree

	Makefile  README.md  shell.c  shelly  tree
			12315 done

			12316 done

			12317 done
	Makefile  README.md  shell.c  shelly  tree

			12318 done

			12319 done

	/home/paulw/repos/COP4600-UnixShell# 	
```

### dalekall
Is also aliased to `killall`.
```sh
	dalekall	
```

Say I ran `tree /` a bunch of times in the background. I can view a list of all
of the currently running background jobs with the `lsbg` command. I can then killed 
them with the `killall` or `dalekall` command:
```sh
	/home/paulw/repos/COP4600-UnixShell# repeat 5 tree / > trees    
	pid: 12559
	pid: 12560
	pid: 12561
	pid: 12562
	pid: 12563
	/home/paulw/repos/COP4600-UnixShell# lsbg
	12563
	12562
	12561
	12560
	12559
	/home/paulw/repos/COP4600-UnixShell# killall
	Killed 12563
	Killed 12562
	Killed 12561
	Killed 12560
	Killed 12559
	/home/paulw/repos/COP4600-UnixShell# 
			12560 done

			12559 done

			12561 done

			12562 done

			12563 done

	/home/paulw/repos/COP4600-UnixShell# 
```

