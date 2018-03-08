#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/epoll.h>

#include <dynamic.h>
#include <reactor.h>

#include "frame.h"
#include "capture.h"
#include "capture_mock.h"

static int capture_event(void *state, int type, void *data)
{
  capture *capture = state;
  char buffer[256];
  ssize_t n;
  capture_frame *p, frame;
  int e;

  if (type != REACTOR_DESCRIPTOR_EVENT_POLL || *(int *) data != EPOLLIN)
    return reactor_user_dispatch(&capture->user, CAPTURE_EVENT_ERROR, NULL);

  n = read(capture->signal[0], buffer, sizeof buffer);
  if (n <= 0)
    return reactor_user_dispatch(&capture->user, CAPTURE_EVENT_ERROR, NULL);

  while (1)
    {
      pthread_mutex_lock(&capture->mutex);
      p = NULL;
      if (!list_empty(&capture->frames))
        {
          p = (capture_frame *) list_front(&capture->frames);
          frame = *p;
          list_erase(p, NULL);
        }
      pthread_mutex_unlock(&capture->mutex);
      if (!p)
        break;
      e = reactor_user_dispatch(&capture->user, CAPTURE_EVENT_FRAME, &frame);
      frame_release(frame.video);
      frame_release(frame.audio);
      if (e != REACTOR_OK)
        return REACTOR_ABORT;
    }

  return REACTOR_OK;
}

int capture_open(capture *c, reactor_user_callback *callback, void *state, int flags)
{
  int e;

  reactor_user_construct(&c->user, callback, state);
  list_construct(&c->frames);
  pthread_mutex_init(&c->mutex, NULL);
  e = pipe(c->signal);
  if (e == -1)
    return -1;

  e = reactor_descriptor_open(&c->descriptor, capture_event, c, c->signal[0], EPOLLIN);
  if (e != REACTOR_OK)
    return -1;

  if (flags & CAPTURE_FLAG_MOCK)
    {
      c->internal = capture_mock_new(c);
      if (!c->internal)
        return -1;
      return 0;
    }

  return -1;
}

void capture_close(capture *c)
{
  capture_frame *f;

  reactor_descriptor_close(&c->descriptor);
  list_foreach(&c->frames, f)
    {
      frame_release(f->audio);
      frame_release(f->video);
    }

  list_clear(&c->frames, NULL);

  if (c->internal)
    capture_mock_free(c->internal);
}
