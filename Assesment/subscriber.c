#include <stdio.h>
#include <stdlib.h>
#include "subscriber_struct.h"


//create new sub channel
subbed_channel* create(int ch_id, subbed_channel* next){
    subbed_channel* new_sub = (subbed_channel*)malloc(sizeof(subbed_channel));

    if(new_sub == NULL){
        printf("Error Subscribing.\n");
        exit(0);
    }

    new_sub->channel_id = ch_id;
    new_sub->next = next;
    new_sub->read_index = 0;

    return new_sub;
}

//ad to the front
subbed_channel* prepend(subbed_channel* head, int ch_id){
    subbed_channel* new_sub = create(ch_id, head);
    head = new_sub;
    return head;
}


int count(subbed_channel *head){
    subbed_channel* cursor = head;
    int c = 0;

    while(cursor != NULL){
        c++;
        cursor = cursor->next;
    }

    return c;
}

void traverse(subbed_channel* head, callback f){
    subbed_channel *cursor = head;

    while(cursor != NULL){
        f(cursor);
        cursor = cursor->next;
    }
}

//add to end
subbed_channel* append(subbed_channel* head, int ch_id){
    subbed_channel* cursor = head;

    while(cursor->next != NULL){
        cursor = cursor->next;
    }

    //create new subbed_channel
    subbed_channel *new_sub = create(ch_id, NULL); //null = points to end
    cursor->next = new_sub;

    return head;
}

subbed_channel* insert_after(subbed_channel *head, int ch_id, subbed_channel *prev){

    //find the preb node, starting from the first node
    subbed_channel *cursor = head;

    while(cursor != prev){
        cursor = cursor->next;
    }

    if(cursor != NULL){
        subbed_channel* new_sub = create(ch_id, cursor->next);
        cursor->next = new_sub;
        return head;
    } else {
        return NULL;
    }
}

subbed_channel* insert_before(subbed_channel *head, int ch_id, subbed_channel *nxt){

    if(nxt == NULL || head == NULL){return NULL;}

    if(head == nxt){
        head = prepend(head, ch_id);
        return head;
    }

    //find the prev channel node, start from the first one

    subbed_channel *cursor = head;

    while(cursor != NULL){
        if(cursor->next == nxt) break;
        cursor = cursor->next;
    }

    if(cursor != NULL){
        subbed_channel *new_sub = create(ch_id, cursor->next);
        cursor->next = new_sub;
        return head;
    } else {
        return NULL;
    }
}

subbed_channel* search(subbed_channel* head, int ch_id){

    //start 
    subbed_channel *cursor = head;

    while(cursor != NULL){
        if(cursor->channel_id == ch_id){return cursor;} //found it
        cursor = cursor->next;
    }

    return NULL;
}

subbed_channel* remove_front(subbed_channel* head){
    
    if(head == NULL){return NULL;}

    subbed_channel *front = head;
    head = head->next;

    front->next = NULL;

    if(front == head){head == NULL;}

    free(front);

    return head;
}

subbed_channel* remove_back(subbed_channel* head){

    if(head == NULL){return NULL;}

    subbed_channel* cursor = head;
    subbed_channel* back = NULL;

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

subbed_channel* remove_sub(subbed_channel* head, subbed_channel* sub){

    //if sub is the first one
    if(sub == head){
        head = remove_front(head);
        return head;
    }

    //if sub is the last one
    if(sub->next == NULL){
        head = remove_back(head);
        return head;
    }

    //somewhere inbetween
    subbed_channel *cursor = head;

    while(cursor != NULL){
        if(cursor->next = sub){break;}
        cursor = cursor->next;
    }

    if(cursor != NULL){
        subbed_channel *tmp = cursor->next;
        cursor->next = tmp->next;
        tmp->next = NULL;
        free(tmp);
    }

    return head;
}

void sub_dispose(subbed_channel* head){

    subbed_channel *cursor, *tmp;

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

subbed_channel* search_remove(subbed_channel* head, int ch_id){
       
        subbed_channel* tmp = search(head, ch_id);
         
        if(tmp != NULL)
        {
            remove_sub(head,tmp);
            
            //if(head != NULL)
                //traverse(head, display);
        }
        else
        {
            printf("Not Subscribed to %d.", ch_id);
        }
}

void display(subbed_channel* sub){

    if(sub != NULL){
        printf("%d\n", sub->channel_id);
    }
}

