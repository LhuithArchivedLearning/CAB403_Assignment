#include <signal.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/types.h> 
#include <unistd.h>

#define FILLED 0 
#define Ready 1 
#define NotReady -1 

//The Actual Message/Post
typedef struct POST_STRUCT{
	int read;
	char message[1024];
} post;

//A Channel with 255 messages
typedef struct CHANNEL_STRUCT {
	post posts[255];
} channel;

//Information about the client?
typedef struct CLIENT_STRUCT {
	char channel_subs[255]; //0 or 1? find something better for this
	int client_id;
} client;

client Clients[255];

//Block of Memory holding 255 channels, all with 255 messages
struct memory { 
    char buff[255]; 
    int status;
    int num_clients;

}; 

//pointer to the memory block
struct memory* shmptr;
int shmid;

void init_shared(){
    
    int pid = getpid(); 

    key_t key; 
    
    if ((key = ftok("/tmp", 'a')) == (key_t) -1) {
    perror("IPC error: ftok"); exit(1);
}

    if((shmid = shmget(key, sizeof(struct memory), IPC_CREAT | 6666) == -1)){
        printf("Error with Shared");
    }

    shmptr = (struct memory *)shmat(shmid, NULL, 0);
    
    if (shmptr == (void *) -1) {
      perror("Shared memory attach");
      return;
   }

    shmptr->num_clients = 1;
    shmptr->status = NotReady;
    //strcpy(shmptr->buff, "Hello World");

    printf("Message Block Created: %d\n", sizeof(struct memory));
};


void clear_shared(){
    shmdt((void*)shmptr);
    shmctl(shmid, IPC_RMID, NULL);

    printf("Message Block Cleared\n");
}


char* getMessage(int channel, int post){
    return shmptr->buff;//[channel].posts[post].message;
}

//void getClient(int i){return Clients[i];}

int getClientID(){return 1;}