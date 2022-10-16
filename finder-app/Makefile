
ifdef CROSS_COMPILE
	GCC=$(CROSS_COMPILE)gcc
else
	GCC=gcc
endif


default:
	$(GCC) -g -Wall -c -o writer.o writer.c
	$(GCC) -g -Wall -I/  writer.o -o writer
	

clean:
	rm -f *.o writer
