#ifndef WORK_JOB_STRUCT
#define WORK_JOB_STRUCT

typedef struct job{
    int job_id;
    char* data;
    struct job *next;
} job;

typedef void (*job_callback)(job* channel);

typedef struct worker{
  job *head;
} worker;

#endif