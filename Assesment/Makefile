#CC = gcc
#
#CFLAGS = -g
#
#TARGET = shared server client
#
#all: $(TARGET)
#
#$(TARGET): %: %.c %.h
#	$(CC) $(CFLAGS) -o $@ $<

#client.o

SERVER_OBJ = server.o shared.o
CLIENT_OBJ = client.o

CC_FLAGS=-std=gnu99 #-Wall -Werror -g #-I../ZDK 
LD_FLAGS=
LIB_SRC = server.c client.c shared.c

all: server client

server: $(SERVER_OBJ) build_server_obj
	gcc $(SERVER_OBJ) $(LD_FLAGS) -o $@

client: $(CLIENT_OBJ) build_client_obj
	gcc $(CLIENT_OBJ) $(LD_FLAGS) -o $@

%.o : %.c
	gcc -c $< $(CC_FLAGS)

build_server_obj :
	gcc -c $(LIB_SRC) $(CC_FLAGS)

build_client_obj :
	gcc -c $(LIB_SRC) $(CC_FLAGS)