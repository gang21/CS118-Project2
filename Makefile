CC=gcc
CFLAGS=-Wall -Wextra -Werror
USERID=005677251

all: clean build

default: build

build: server.c client.c
	gcc -Wall -Wextra -o server server.c
	gcc -Wall -Wextra -o client client.c

clean:
	rm -f server client output.txt

zip: clean
	zip ${USERID}.zip server.c client.c Makefile utils.h report.txt