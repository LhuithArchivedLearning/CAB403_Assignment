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

SERVER_OBJ = server.o shared.o subscriber.o helper.o printf_helper.o
CLIENT_OBJ = client.o printf_helper.o helper.o

CC_FLAGS=-std=gnu99 -D_XOPEN_SOURCE=500#-Wall -Werror -g #-I../ZDK 
LD_FLAGS=-lrt -pthread
LIB_SRC = server.c client.c shared.c subscriber.c helper.c printf_helper.c

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