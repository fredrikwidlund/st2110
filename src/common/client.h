#ifndef CLIENT_H_INCLUDED
#define CLIENT_H_INCLUDED

enum client_event
{
  CLIENT_EVENT_ERROR,
  CLIENT_EVENT_FRAME
};

enum client_flag
{
  CLIENT_FLAG_STEAL_REFERENCE = (1 << 0),
  CLIENT_FLAG_STATIC_DATA     = (1 << 1)
};

typedef struct client client;

struct client
{
  reactor_user        user;
  reactor_descriptor  descriptor;
  size_t              size;
  size_t              offset;
  frame_pool          pool;
  frame              *frame;
};

int  client_open(client *, reactor_user_callback *, void *, char *, size_t);
void client_close(client *);

#endif /* CLIENT_H_INCLUDED */
