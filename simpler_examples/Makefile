CC = gcc
CFLAGS = -Werror -Wall
includes = ./hiredis ./rmnotify/src
libs = hiredis event notify pthread

OBJECTS = shr_mem \
		redis_client_test \
		redis_keyspace_notification \

SRCS = $(wildcard *.c)

# all: $(SRCS:.c=)

# .c:
# 	gcc $(CFLAGS) $< -o $@ $(foreach incl, $(includes), $(addprefix -I, $(incl))) $(foreach lib, $(libs), $(addprefix -l, $(lib)))

all: shr_mem redis_client_test redis_keyspace_notification

shr_mem: shr_mem.c
	$(CC) $(CFLAGS) $< -o $@

redis_client_test: redis_client_test.c
	$(CC) $(CFLAGS) -I./hiredis -lhiredis $< -o $@

redis_keyspace_notification: redis_keyspace_notification.c
	$(CC) $(CFLAGS) -I./hiredis $< -o $@ -levent -lhiredis

clean:
	@rm -rf $(OBJECTS)

test: $(OBJECTS)
	@$(foreach exec, $(OBJECTS), $(addprefix ./, $(exec)))
