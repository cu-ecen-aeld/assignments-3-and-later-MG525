
ifndef CC
	ifdef CROSS_COMPILE
		CC=$(CROSS_COMPILE)gcc
	else
		CC=gcc
	endif
endif

all: clean default

default:
	$(CC) -g -Wall -c -o writer.o writer.c
	$(CC) -g -Wall -I/  writer.o -o writer
	

clean:
	rm -f *.o writer
