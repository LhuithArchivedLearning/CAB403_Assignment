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

struct shared {
    sem_t mutex;
    int count;
} shared;

#define FILE_MODE   (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

int main(int argc, char **argv){
    if(argc != 3){
        printf("usage: incr3 <pathname> <#loops>");
        exit(0);
    }


    int fd, i, nloops;
    struct shared *ptr;

    nloops = atoi(argv[2]);

    //fd = open(argv[1], O_RDWR | O_CREAT, FILE_MODE);
   fd = shm_open("/myregion", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    
    if(fd == -1){printf("the fuck?");}
    if(ftruncate(fd, (sizeof(struct shared)) == -1)){printf("double fuck?");}

    //write(fd, &shared, sizeof(struct shared));
    
    ptr = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED){printf("doh");}
    //close(fd);

    sem_init(&ptr->mutex, 1, 1);
    //setbuf(stdout, NULL);

    if(fork() == 0){
        for(int i = 0; i < nloops; i++){
            sem_wait(&ptr->mutex);
            printf("child %d\n", ptr->count++);
            sem_post(&ptr->mutex);
        }
        exit(0);
    }

     for(int i = 0; i < nloops; i++){
            sem_wait(&ptr->mutex);
            printf("parent %d\n", ptr->count++);
            sem_post(&ptr->mutex);
    }
    
    //munmap(ptr, (sizeof(struct shared)));
    //shm_unlink("/myregion");

    return 0;
}