#ifndef WORKER_H
#define WORKER_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "worker_job_struct.h"

job* job_create(int j_id, char* d, job* next);
job* job_prepend(job* head, int j_id, char* d);
int job_count(job *head);
void job_traverse(job* head, job_callback f);
job* job_append(job* head, int j_id, char* d);
job* job_insert_after(job *head, int j_id, char* d, job *prev);
job* job_insert_before(job *head, int j_id, char* d, job *nxt);
job* job_search(job* head, int j_id, char* d);
job* job_remove_front(job* head);
job* job_remove_back(job* head);
job* remove_job(job* head, job* jb);
void job_dispose(job* head);
job* job_search_remove(job* head, int j_id, char* d);
void job_display(job* jb);
int return_job_id(job* jb);

#endif