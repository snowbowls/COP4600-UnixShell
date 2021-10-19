build:
	gcc -lreadline -o shelly shell.c

run: build
	./shelly


clean:
	rm shelly a.out
