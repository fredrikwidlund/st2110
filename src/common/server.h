#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#define SERVER_CLIENT_BUFFER_LIMIT (512 * 1024 * 1024)

enum server_event
{
  SERVER_EVENT_ERROR,
  SERVER_EVENT_LOG
};

enum server_flag
{
  SERVER_FLAG_STEAL_REFERENCE = (1 << 0)
};

enum server_client_status
{
  SERVER_CLIENT_STATUS_STREAMING,
  SERVER_CLIENT_STATUS_BUFFERING
};

typedef struct server_client server_client;
typedef struct server server;

struct server_client
{
  size_t              id;
  int                 status;
  server             *server;
  reactor_descriptor  descriptor;
  size_t              offset;
  size_t              buffered;
  list                queue;
};

struct server
{
  reactor_user        user;
  reactor_descriptor  descriptor;
  char               *name;
  size_t              id;
  double              sync;
  server             *master;
  uint64_t            deadline;
  list                clients;
  list                backlog;
  list                slaves;
};

int           server_open(server *, reactor_user_callback *, void *, char *, double);
void          server_sync(server *, server *);
void          server_distribute(server *, frame *);
void          server_close(server *);

#endif /* SERVER_H_INCLUDED */
