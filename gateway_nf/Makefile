CC = gcc
CFLAGS = -Wall -Werror
INCLUDES = ../hiredis
LIBS = hiredis

gateway_nf.o: test.c
	@$(CC) $(CFLAGS) -I$(INCLUDES) $< -o $@ -l$(LIBS)

clean:
	@rm -rf *.o