#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "subscriber_struct.h"

subbed_channel* create(int ch_id, subbed_channel* next, subbed_channel* client_head);
subbed_channel* prepend(subbed_channel* head, int ch_id);
int count(subbed_channel* head);
void traverse(subbed_channel* head, callback f);
subbed_channel* append(subbed_channel* head, int ch_id);
subbed_channel* insert_after(subbed_channel* head, int ch_id, subbed_channel *prev);
subbed_channel* insert_before(subbed_channel* head, int ch_id, subbed_channel *nxt);
subbed_channel* search(subbed_channel* head, int ch_id);
subbed_channel* remove_front(subbed_channel* head);
subbed_channel* remove_back(subbed_channel* head);
subbed_channel* remove_sub(subbed_channel* head, subbed_channel* sub);
void sub_dispose(subbed_channel* head);
subbed_channel* search_remove(subbed_channel* head, int ch_id);
void display(subbed_channel* sub);