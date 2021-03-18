CC = gcc
CFLAGS = -Werror -Wall

OBJECTS = shr_mem \

SRCS = $(wildcard *.c)

all: $(SRCS:.c=)

.c:
	@gcc $(CFLAGS) $< -o $@

clean:
	@rm -rf $(OBJECTS)

test: $(OBJECTS)
	@$(foreach exec, $(OBJECTS), $(addprefix ./, $(exec)))
