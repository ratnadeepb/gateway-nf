CC = gcc
CFLAGS = -Wall -Werror
INCLUDES = ../hiredis
LIBS = hiredis pthread

OBJECTS = gateway_nf.o \
		client_nf.o \

all: $(OBJECTS)

gateway_nf.o: test.c
	@$(CC) $(CFLAGS) -I$(INCLUDES) $< -o $@ $(foreach lib, $(LIBS), $(addprefix -l, $(lib)))

client_nf.o: test_client_nf.c
	@$(CC) $(CFLAGS) -I$(INCLUDES) $< -o $@ $(foreach lib, $(LIBS), $(addprefix -l, $(lib)))

clean:
	@rm -rf *.o