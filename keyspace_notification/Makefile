CC = gcc
CFLAGS = -Wall -Werror
INCLUDES = -I../hiredis
LIBS = -lhiredis -lpthread -levent

OBJ = keyspacenotification.o
# SO = libnotify.so
STLIBNAME = libnotify.a

all: $(STLIBNAME)

$(OBJ): keyspacenotification.c
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ $(LIBS)

$(STLIBNAME): $(OBJ)
	@ar rcs $(STLIBNAME) $(OBJ)

clean:
	@rm -rf *.o
	@rm -rf *.so