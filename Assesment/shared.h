#include <signal.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/types.h> 
#include <unistd.h>

//Shared Memery Init
void init_shared();

//Shared Memory Clear
void clear_shared();

//client getClient(int i);
int getClientID();
char* getMessage(int channel, int post);

