
ifndef CC
	ifdef CROSS_COMPILE
		CC=$(CROSS_COMPILE)gcc
	else
		CC=gcc
	endif
endif

all: clean default

default:
	$(CC) -g -Wall -Werror -c -o aesdsocket.o aesdsocket.c
	$(CC) -g -Wall -Werror -I/  aesdsocket.o -o aesdsocket
	

clean:
	rm -f *.o aesdsocket
