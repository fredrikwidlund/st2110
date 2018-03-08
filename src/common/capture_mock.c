#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <sys/uio.h>
#include <sys/epoll.h>

#include <dynamic.h>
#include <reactor.h>

#include "frame.h"
#include "capture.h"

typedef struct capture_mock capture_mock;
struct capture_mock
{
  capture       *capture;
  reactor_timer  timer;
};

static int capture_mock_event(void *state, int type, void *data)
{
  capture_mock *m = state;
  capture_frame frame;
  uint64_t i, exp;
  ssize_t n;

  if (type != REACTOR_TIMER_EVENT_CALL)
    abort();
  exp = *(uint64_t *) data;

  for (i = 0; i < exp; i ++)
    {
      frame.video = frame_new(sizeof *frame.video);
      frame_reserve(frame.video, 1280 * 720 * 4 / 2);
      frame.video->data = frame.video->memory;
      memset(frame_data(frame.video), 0, frame_size(frame.video));

      frame.audio = frame_new(sizeof *frame.audio);
      frame_reserve(frame.audio, 960 * 2 * 2);
      frame.audio->data = frame.audio->memory;
      memset(frame_data(frame.audio), 0, frame_size(frame.audio));

      pthread_mutex_lock(&m->capture->mutex);
      list_push_back(&m->capture->frames, &frame, sizeof frame);
      pthread_mutex_unlock(&m->capture->mutex);
    }

  n = write(m->capture->signal[1], "", 1);
  if (n != 1)
    abort();

  return REACTOR_OK;
}

void *capture_mock_new(capture *capture)
{
  capture_mock *m;
  int e;
  uint64_t rate;

  m = malloc(sizeof *m);
  if (!m)
    abort();
  m->capture = capture;

  rate = 1000000000/50;
  e = reactor_timer_open(&m->timer, capture_mock_event, m, rate, rate);
  if (e != REACTOR_OK)
    {
      free(m);
      return NULL;
    }

  return m;
}

void capture_mock_free(void *m)
{
  reactor_timer_close(&((capture_mock *) m)->timer);
  free(m);
}
