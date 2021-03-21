CC = gcc
CFLAGS = -Wall -Werror
INCLUDES = ../hiredis
LIBS = hiredis pthread event

OBJECTS = keyspacenotification.o
SO = keyspacenotification.so

all: $(OBJECTS)

keyspacenotification.o: keyspacenotification.c
	@$(CC) -fPIC $(CFLAGS) -I$(INCLUDES) -c $< -o $@ $(foreach lib, $(LIBS), $(addprefix -l, $(lib)))
	$(CC) -shared -o $(SO) $@

clean:
	@rm -rf *.o
	@rm -rf *.so