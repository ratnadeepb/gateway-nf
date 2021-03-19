CC = gcc
CFLAGS = -Werror -Wall
includes = hiredis-1.0.0
libs = hiredis

OBJECTS = shr_mem \
			redis_client_test \

SRCS = $(wildcard *.c)

all: $(SRCS:.c=)

.c:
	@gcc $(CFLAGS) $< -o $@ -l$(libs)

clean:
	@rm -rf $(OBJECTS)

test: $(OBJECTS)
	@$(foreach exec, $(OBJECTS), $(addprefix ./, $(exec)))
