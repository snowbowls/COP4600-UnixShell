build:
	gcc -lreadline -o shelly shell.c

run: build
	./shelly


debug:
	gcc -g -lreadline -o shelly shell.c

memdebug: debug
	valgrind --leak-check=full ./shelly

clean:
	rm shelly a.out
