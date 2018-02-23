#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <limits.h>
#include <fcntl.h>
#include <err.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <dynamic.h>
#include <reactor.h>

#include "frame.h"
#include "packet.h"
#include "rtp.h"
#include "reader.h"

typedef struct app app;
struct app
{
  reader audio;
  reader video;
};

int audio(void *state, int type, void *data)
{
  app *app = state;
  reader_frame *f;

  if (type != READER_EVENT_DATA)
    err(1, "audio event");

  while (1)
    {
      f = reader_front(&app->audio);
      if (!f)
        break;
      //printf("audio %lu %lu\n", f->time, f->duration);
      reader_pop(&app->audio);
    }

  return REACTOR_OK;
}

int video(void *state, int type, void *data)
{
  app *app = state;
  reader_frame *f;

  if (type != READER_EVENT_DATA)
    err(1, "video event");

  while (1)
    {
      f = reader_front(&app->video);
      if (!f)
        break;
      //printf("video %lu %lu\n", f->time, f->duration);
      reader_pop(&app->video);
    }

  return REACTOR_OK;
}

int main(int argc, char **argv)
{
  app app;
  int e;

  reactor_core_construct();

  e = reader_open(&app.video, video, &app, argv[1], argv[2]);
  if (e != REACTOR_OK)
    err(1, "reader_open_video");
  reader_type_video(&app.video, 1280, 720, 4, 2, 1800);

  e = reader_open(&app.audio, audio, &app, argv[1], argv[3]);
  if (e != REACTOR_OK)
    err(1, "reader_open_audio");
  reader_type_audio(&app.audio, 2, 2, 1800);

  e = reactor_core_run();
  if (e != REACTOR_OK)
    err(1, "reactor_core_run");

  reactor_core_destruct();
}
