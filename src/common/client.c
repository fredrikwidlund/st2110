#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
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
#include "client.h"

static int client_error(client *c)
{
  return reactor_user_dispatch(&c->user, CLIENT_EVENT_ERROR, NULL);
}

static int client_event(void *state, int type, void *data)
{
  client *c = state;
  frame *f;
  int e, *flags = data;
  ssize_t n;

  if (type != REACTOR_DESCRIPTOR_EVENT_POLL || *flags & ~EPOLLIN)
    return client_error(c);

  while (1)
    {
      f = c->frame;
      if (!f)
        {
          c->frame = frame_pool_new(&c->pool);
          frame_reserve(c->frame, c->size);
          c->frame->data = c->frame->memory;
          f = c->frame;
        }

      n = read(reactor_descriptor_fd(&c->descriptor), (uint8_t *) frame_data(f) + c->offset, frame_size(f) - c->offset);
      if (n <= 0)
        {
          if (n == -1 && errno == EAGAIN)
            return REACTOR_OK;
          return client_error(c);
        }
      c->offset += n;

      if (c->offset == frame_size(f))
        {
          c->offset = 0;
          c->frame = NULL;
          e = reactor_user_dispatch(&c->user, CLIENT_EVENT_FRAME, f);
          frame_release(f);
          if (e == REACTOR_ABORT)
            return REACTOR_ABORT;
        }
    }
}

static int client_connect(client *c, char *name)
{
  struct sockaddr_un sun;
  int e, fd;

  sun = (struct sockaddr_un) {0};
  sun.sun_family = AF_UNIX;
  strncpy(sun.sun_path, name, sizeof sun.sun_path - 1);
  if (sun.sun_path[0] == '@')
    sun.sun_path[0] = 0;

  fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (fd == -1)
    return -1;

  e = connect(fd, (struct sockaddr *) &sun, offsetof(struct sockaddr_un, sun_path) + strlen(name));
  if (e == -1)
    {
      (void) close(fd);
      return -1;
    }

  e = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (int[]) {INT_MAX}, sizeof (int));
  if (e == -1)
    {
      (void) close(fd);
      return -1;
    }

  return reactor_descriptor_open(&c->descriptor, client_event, c, fd, EPOLLIN | EPOLLET);
}

int client_open(client *c, reactor_user_callback *callback, void *state, char *name, size_t size)
{
  int e;

  *c = (client) {.size = size};
  reactor_user_construct(&c->user, callback, state);
  frame_pool_construct(&c->pool, sizeof *c->frame);

  e = client_connect(c, name);
  if (e == -1)
    {
      client_close(c);
      return -1;
    }

  return 0;
}

void client_close(client *c)
{
  reactor_descriptor_close(&c->descriptor);
}
