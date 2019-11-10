#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include "shared_mem_struct.h"
#include <semaphore.h>

int fd;
char* memaddr;
struct memory* memptr;

#define FILE_MODE   (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

void* create_shared_mem(char* addr){
    memaddr = addr;

    int struct_size = sizeof(struct memory);

    fd = shm_open(addr, O_CREAT | O_RDWR, FILE_MODE);
    if(fd == -1){perror("fd: "); exit(0);}

    ftruncate(fd, struct_size); //set size 
    memptr = mmap(NULL, struct_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(memptr == MAP_FAILED){perror("memory allocation error: "); exit(0);}
    
    size_t n = sizeof(memptr->channels)/sizeof(memptr->channels[0]);
    
    //initing mutex locks per channel
    for(int i = 0; i < n; i++){
        sem_init(&memptr->channels[i].mutex, 0, 0);
    }

    close(fd);
    printf("Memory Block Created: %d\n", struct_size);
    return memptr;
};


void clean_up_shared_mem(){

    size_t n = sizeof(memptr->channels)/sizeof(memptr->channels[0]);
    
    //destroying mutex locks per channel
    for(int i = 0; i < n; i++){
        sem_destroy(&memptr->channels[i].mutex);
    }

    munmap(memptr, sizeof(struct memory));
    shm_unlink(memaddr);
    printf("Memory Block Cleared\n");
}

int getClientID(){return 1;}