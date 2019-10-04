#ifndef MEM_STRUCT
#define MEM_STRUCT

#include <semaphore.h>

//The Actual Message/Post
typedef struct POST_STRUCT{
	char read; //0 or 1 //read or not read
    sem_t mutex; //set up critical section for posts
	char message[1024];
} post;


struct memory { 
    char buff[255]; 
    int status; // is someone reading to or writing too? 
    int num_clients;
}; 

#endif