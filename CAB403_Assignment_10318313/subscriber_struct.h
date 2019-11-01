#ifndef SUB_STRUCT
#define SUB_STRUCT

typedef struct subbed_channel{
    int channel_id;
    int read_index;
    struct subbed_channel *next;
} subbed_channel;

typedef void (*callback)(subbed_channel* channel);

typedef struct client{
  subbed_channel *head;
  int client_id;
} client;

#endif
