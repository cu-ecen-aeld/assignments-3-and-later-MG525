

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif
ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror
endif
ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif



all: clean default

default:
	$(CC) $(LDFLAGS) $(CFLAGS) -D USE_AESD_CHAR_DEVICE=1 -c -o aesdsocket.o aesdsocket.c
	$(CC) $(LDFLAGS) $(CFLAGS) -I/  aesdsocket.o -o aesdsocket
	

clean:
	rm -f *.o aesdsocket
