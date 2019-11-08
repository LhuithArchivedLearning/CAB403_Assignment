#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "worker_job_struct.h"


//create new jb channel
job* job_create(int j_id, char* d, job* next){
    job* new_job = (job*)malloc(sizeof(job));

    if(new_job == NULL){
        printf("Error Subscribing.\n");
        exit(0);
    }

    new_job->job_id = j_id;
    new_job->next = next;
    new_job->data = malloc(sizeof(d));
    strcpy(new_job->data, d);
    return new_job;
}

//ad to the front
job* job_prepend(job* head, int j_id, char* d){
    job* new_job = job_create(j_id, d, head);
    //printf("%s", d);
    head = new_job;
    return head;
}


int job_count(job *head){
    job* cursor = head;
    int c = 0;

    while(cursor != NULL){
        c++;
        cursor = cursor->next;
    }

    return c;
}

void job_traverse(job* head, job_callback f){
    job *cursor = head;

    while(cursor != NULL){
        f(cursor);
        cursor = cursor->next;
    }
}

//add to end
job* job_append(job* head, int j_id, char* d){
    job* cursor = head;

    while(cursor->next != NULL){
        cursor = cursor->next;
    }

    //create new job
    job *new_job = job_create(j_id, d, NULL); //null = points to end
    cursor->next = new_job;

    return head;
}

job* job_insert_after(job *head, int j_id, char* d, job *prev){

    //find the preb node, starting from the first node
    job *cursor = head;

    while(cursor != prev){
        cursor = cursor->next;
    }

    if(cursor != NULL){
        job* new_job = job_create(j_id, d, cursor->next);
        cursor->next = new_job;
        return head;
    } else {
        return NULL;
    }
}

job* job_insert_before(job *head, int j_id, char* d, job *nxt){

    if(nxt == NULL || head == NULL){return NULL;}

    if(head == nxt){
        head = job_prepend(head, j_id, d);
        return head;
    }

    //find the prev channel node, start from the first one

    job *cursor = head;

    while(cursor != NULL){
        if(cursor->next == nxt) break;
        cursor = cursor->next;
    }

    if(cursor != NULL){
        job *new_job = job_create(j_id, d, cursor->next);
        cursor->next = new_job;
        return head;
    } else {
        return NULL;
    }
}


job* job_search(job* head, int j_id, char* d){

    //start 
    job *cursor = head;

    while(cursor != NULL){
        if(cursor->job_id == j_id){return cursor;} //found it
        cursor = cursor->next;
    }

    return NULL;
}

job* job_remove_front(job* head){
    
    if(head == NULL){return NULL;}

    job *front = head;
    head = head->next;

    front->next = NULL;

    if(front == head){head == NULL;}

    free(front);

    return head;
}

job* job_remove_back(job* head){

    if(head == NULL){return NULL;}

    job* cursor = head;
    job* back = NULL;

    while(cursor->next != NULL){
        back = cursor;
        cursor = cursor->next;
    }

    if(back != NULL){back->next = NULL;}

    //if last
    if(cursor == head){head = NULL;}

    free(cursor);

    return head;
}

job* remove_job(job* head, job* jb){

    //if jb is the first one
    if(jb == head){
        head = job_remove_front(head);
        return head;
    }

    //if jb is the last one
    if(jb->next == NULL){
        head = job_remove_back(head);
        return head;
    }

    //somewhere inbetween
    job *cursor = head;

    while(cursor != NULL){
        if(cursor->next = jb){break;}
        cursor = cursor->next;
    }

    if(cursor != NULL){
        job *tmp = cursor->next;
        cursor->next = tmp->next;
        tmp->next = NULL;

        free(tmp->data);
        free(tmp);
    }

    return head;
}

void job_dispose(job* head){

    job *cursor, *tmp;

    if(head != NULL){
        cursor = head->next;
        head->next = NULL;

        while(cursor != NULL){
            tmp = cursor->next;
            free(cursor);
            cursor = tmp;
        }
    }
}

job* job_search_remove(job* head, int j_id, char* d){
       
        job* tmp = job_search(head, j_id, d);
         
        if(tmp != NULL)
        {
            remove_job(head, tmp);
            
            //if(head != NULL)
                //traverse(head, display);
        }
        else
        {
            printf("Not Subscribed to %d.", j_id);
        }
}

void job_display(job* jb){

    if(jb != NULL){
        printf("%d\n", jb->job_id);
    }
}

int return_job_id(job* jb){
    if(jb != NULL){
        return jb->job_id;
    } 
}

