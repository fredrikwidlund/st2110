#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <dynamic.h>
#include <reactor.h>

#include "frame.h"
#include "server.h"

static void server_close_client(server *, server_client *);
static int server_client_flush(server_client *);

static int server_error(server *s)
{
  return reactor_user_dispatch(&s->user, SERVER_EVENT_ERROR, NULL);
}

static int server_log(server *s, const char *format, ...)
{
  char reason[4096];
  va_list ap;

  va_start(ap, format);
  (void) vsnprintf(reason, sizeof reason, format, ap);
  va_end(ap);

  return reactor_user_dispatch(&s->user, SERVER_EVENT_LOG, reason);
}

static uint64_t server_ntime(void)
{
  struct timespec ts;

  (void) clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return ((uint64_t) ts.tv_sec * 1000000000) + ((uint64_t) ts.tv_nsec);
}

static void server_client_free(server_client *c)
{
  frame **f;

  list_foreach(&c->queue, f)
    frame_release(*f);
  list_destruct(&c->queue, NULL);
  reactor_descriptor_close(&c->descriptor);
  free(c);
}

static int server_client_event(void *state, int type, void *data)
{
  server_client *c = state;
  int e, *flags = data;
  char buffer[1024*1024];
  ssize_t n;

  if (type != REACTOR_DESCRIPTOR_EVENT_POLL || *flags & ~(EPOLLIN | EPOLLOUT))
    {
      server_close_client(c->server, c);
      return REACTOR_ABORT;
    }

  if (*flags & EPOLLIN)
    {
      n = read(reactor_descriptor_fd(&c->descriptor), buffer, sizeof buffer);
      if (n == 0)
        {
          server_close_client(c->server, c);
          return REACTOR_ABORT;
        }
    }

  if (*flags & EPOLLOUT)
    {
      e = server_client_flush(c);
      if (e == -1)
        return REACTOR_ABORT;
      if (!c->buffered)
        {
          c->status = SERVER_CLIENT_STATUS_STREAMING;
          reactor_descriptor_set(&c->descriptor, EPOLLIN);
          (void) server_log(c->server, "[%s] consumer %lu recovered", c->server->name, c->id);
        }
    }

  return REACTOR_OK;
}

static server_client *server_client_new(server *s, int id, int fd)
{
  server_client *c;
  int e;

  c = calloc(1, sizeof *c);
  if (!c)
    abort();

  c->id = id;
  c->status = SERVER_CLIENT_STATUS_STREAMING;
  c->server = s;
  c->offset = 0;
  c->buffered = 0;
  list_construct(&c->queue);

  e = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (int[]) {INT_MAX}, sizeof (int));
  if (e == -1)
    {
      (void) close(fd);
      server_client_free(c);
      return NULL;
    }

  e = reactor_descriptor_open(&c->descriptor, server_client_event, c, fd, EPOLLIN);
  if (e != REACTOR_OK)
    {
      (void) close(fd);
      server_client_free(c);
      return NULL;
    }

  return c;
}

static void server_clients_release(void *p)
{
  server_client_free(*(server_client **) p);
}

static int server_client_flush(server_client *c)
{
  frame **i, *f;
  ssize_t n;

  while (!list_empty(&c->queue))
    {
      i = list_front(&c->queue);
      f = *i;
      n = write(reactor_descriptor_fd(&c->descriptor),
                (uint8_t *) frame_data(f) + c->offset, frame_size(f) - c->offset);
      if (n == -1)
        {
          if (errno == EAGAIN && c->buffered < SERVER_CLIENT_BUFFER_LIMIT)
            {
              if (c->status == SERVER_CLIENT_STATUS_STREAMING)
                {
                  c->status = SERVER_CLIENT_STATUS_BUFFERING;
                  reactor_descriptor_set(&c->descriptor, EPOLLIN | EPOLLOUT);
                  (void) server_log(c->server, "[%s] consumer %lu buffering", c->server->name, c->id);
                }
              return 0;
            }
          else
            {
              server_close_client(c->server, c);
              return -1;
            }
        }
      c->buffered -= n;
      c->offset += n;
      if (c->offset == frame_size(f))
        {
          frame_release(f);
          list_erase(i, NULL);
          c->offset = 0;
        }
    }

  return 0;
}

static void server_client_distribute(server_client *c, frame *f)
{
  frame_hold(f);
  list_push_back(&c->queue, &f, sizeof f);
  c->buffered += frame_size(f);
}

static server *server_master(server *s)
{
  while (s->master)
    s = s->master;
  return s;
}

static void server_close_client(server *s, server_client *c)
{
  server_client **i;

  (void) server_log(s, "[%s] consumer %lu disconnected", s->name, c->id);
  list_foreach(&s->clients, i)
    if (*i == c)
      {
        list_erase(i, NULL);
        break;
      }
  server_client_free(c);
}

static int server_accept(void *state, int type, void *data)
{
  server *s = state, *master;
  server_client *c;
  frame **f;
  int fd;

  if (type != REACTOR_DESCRIPTOR_EVENT_POLL || *(int *) data != EPOLLIN)
    return server_error(s);

  while (1)
    {
      fd = accept(reactor_descriptor_fd(&s->descriptor), NULL, NULL);
      if (fd == -1)
        return errno == EAGAIN ? REACTOR_OK : server_error(s);

      c = server_client_new(s, s->id, fd);
      if (!c)
        continue;
      s->id ++;

      list_foreach(&s->backlog, f)
        server_client_distribute(c, *f);

      list_push_back(&s->clients, &c, sizeof c);
      master = server_master(s);
      master->deadline = server_ntime() + (master->sync * 1000000000);
      (void) server_log(s, "[%s] consumer %lu connected", s->name, c->id);
    }
}

static int server_connect(server *s, char *name)
{
  struct stat st;
  struct sockaddr_un sun;
  int e, fd;

  sun = (struct sockaddr_un) {0};
  sun.sun_family = AF_UNIX;
  strncpy(sun.sun_path, name, sizeof sun.sun_path - 1);
  if (sun.sun_path[0] != '@')
    {
      e = stat(name, &st);
      if (e == -1 && errno != ENOENT)
        return -1;
      if (e == 0)
        {
          if ((st.st_mode & S_IFMT) != S_IFSOCK)
            return -1;
          unlink(name);
        }
    }
  else
    sun.sun_path[0] = 0;

  fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (fd == -1)
    return -1;

  e = bind(fd, (struct sockaddr *) &sun, offsetof(struct sockaddr_un, sun_path) + strlen(name));
  if (e == -1)
    {
      (void) close(fd);
      return -1;
    }

  e = listen(fd, -1);
  if (e == -1)
    {
      (void) close(fd);
      return -1;
    }

  return reactor_descriptor_open(&s->descriptor, server_accept, s, fd, EPOLLIN);
}

int server_open(server *s, reactor_user_callback *callback, void *state, char *name, double sync)
{
  int e;

  *s = (server) {.sync = sync};
  reactor_user_construct(&s->user, callback, state);
  s->name = strdup(name);
  if (!s->name)
    abort();
  list_construct(&s->clients);
  list_construct(&s->backlog);
  list_construct(&s->slaves);

  e = server_connect(s, name);
  if (e == -1)
    {
      server_close(s);
      return -1;
    }

  return 0;
}

static void server_flush(server *s)
{
  frame **f;

  list_foreach(&s->backlog, f)
    frame_release(*f);
  list_clear(&s->backlog, NULL);
}

static void server_sync_master(server *s)
{
  server **i;

  if (!s->deadline || server_ntime() < s->deadline)
    return;

  (void) server_log(s, "[master] deadline");
  s->deadline = 0;
  server_flush(s);
  list_foreach(&s->slaves, i)
    server_flush(*i);
}

void server_sync(server *s, server *node)
{
  server *master = server_master(node);
  s->master = master;
  list_push_back(&master->slaves, &s, sizeof s);
}

void server_distribute(server *s, frame *f)
{
  server *master = server_master(s);
  server_client **i, **i_next;

  server_sync_master(master);
  if (master->deadline)
    {
      frame_hold(f);
      list_push_back(&s->backlog, &f, sizeof f);
    }

  list_foreach(&s->clients, i)
    server_client_distribute(*i, f);


  i = list_front(&s->clients);
  while (i != list_end(&s->clients))
    {
      i_next = list_next(i);
      if ((*i)->status == SERVER_CLIENT_STATUS_STREAMING)
        (void) server_client_flush(*i);
      i = i_next;
    }
}

void server_close(server *s)
{
  reactor_descriptor_close(&s->descriptor);
  list_destruct(&s->clients, server_clients_release);
  server_flush(s);
  free(s->name);
  list_clear(&s->slaves, NULL);
}
