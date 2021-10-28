build:
	gcc -lreadline -pthread -lpthread -o shelly shell.c

run: build
	./shelly

build-debug:
	gcc -g -pthread -lpthread -lreadline -o shelly shell.c

debug: build-debug
	valgrind --track-origins=yes --leak-check=full ./shelly

clean:
	rm shelly a.out
