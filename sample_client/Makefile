CC = gcc
CFLAGS = -Wall -Werror
INCLUDES = -I../keyspace_notification -I../hiredis
LIBS = -L../keyspace_notification -lnotify -lhiredis -lpthread -levent

OBJECTS = sample_client.o

all: $(OBJECTS)

sample_client.o: sample_client.c
	@$(CC) $(CFLAGS) $< -o $@ $(INCLUDES) $(LIBS)

# sample_client.o: sample_client.c $(OBJECT_FILE)
# 	@$(CC) $(CFLAGS) $< -o $@ -I../keyspace_notification

clean:
	@rm -rf *.o