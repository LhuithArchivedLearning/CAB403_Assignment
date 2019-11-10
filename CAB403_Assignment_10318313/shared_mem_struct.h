#ifndef MEM_STRUCT
#define MEM_STRUCT

#include <semaphore.h>

//The Actual Message/Post
typedef struct POST_STRUCT{
	char read; //0 or 1 //read or not read
	char message[1024];
} post;

typedef struct CHANNEL_STRUCT{

    post posts[255];
    int post_index;
    sem_t mutex; //set up critical section for channel

} channel;

struct memory { 
    channel channels[255]; 
    int status; // is someone reading to or writing too? 
    int num_clients;
}; 

#endif