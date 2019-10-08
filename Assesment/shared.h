#ifndef SHARED_H
#define SHARED_H

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

//Shared Memery Init
void* create_shared_mem(char* addr);

//Shared Memory Cleanup
void clean_up_shared_mem();

#endif
