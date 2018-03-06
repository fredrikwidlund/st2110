#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>

#include <dynamic.h>
#include <reactor.h>

#include "frame.h"
#include "capture.h"
#include "app.h"

static int app_server_event(void *state, int type, void *data)
{
  (void) state;
  (void) type;
  (void) data;
  err(1, "server error");
}

static int app_capture_event(void *state, int type, void *data)
{
  app *app = state;
  capture_frame *frame;

  if (type != CAPTURE_EVENT_FRAME)
    err(1, "capture error");
  frame = data;

  if (frame->audio)
    {
      // sf = server_frame_new(pcm_frame_data(frame->audio), pcm_frame_size(frame->audio),
      //                      SERVER_FLAG_STEAL_REFERENCE);
      //frame->audio->data = NULL;
      //server_distribute(&app->audio_server, sf);
      //server_frame_release(sf);
    }

  if (frame->video)
    {
      //sf = server_frame_new(yuv_frame_data(frame->video), yuv_frame_size(frame->video),
      //                      SERVER_FLAG_STEAL_REFERENCE);
      //frame->video->data = NULL;
      //server_distribute(&app->video_server, sf);
      //server_frame_release(sf);
    }

  return REACTOR_OK;
}

void app_usage(void)
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s [OPTION]...\n", __progname);
  (void) fprintf(stderr, "Frame server sending SDI audio and video to consumers\n");
  (void) fprintf(stderr, "\n");
  (void) fprintf(stderr, "Options:\n");
  (void) fprintf(stderr, "    -a NAME                 audio domain socket path (defaults to /tmp/audio)\n");
  (void) fprintf(stderr, "    -v NAME                 video domain socket path (defaults to /tmp/video)\n");
  (void) fprintf(stderr, "    -s SECONDS              multiple consumer sync window time (defaults to 5.0 seconds)\n");
  (void) fprintf(stderr, "    -m                      mock sdi input\n");
  (void) fprintf(stderr, "    -d                      run in background\n");
  (void) fprintf(stderr, "    -h                      display this help\n");

  exit(EXIT_FAILURE);
}

int app_construct(app *app, int argc, char **argv)
{
  int c;

  *app = (struct app) {.audio_endpoint = "/tmp/audio", .video_endpoint = "/tmp/video", .sync = 5.0};

  while (1)
    {
      c = getopt(argc, argv, "a:v:s:mdh");
      if (c == -1)
        break;

      switch (c)
        {
        case 'a':
          app->audio_endpoint = optarg;
          break;
        case 'v':
          app->video_endpoint = optarg;
          break;
        case 's':
          app->sync = strtod(optarg, NULL);
          break;
        case 'm':
          app->flags |= APP_FLAG_MOCK_SDI;
          break;
        case 'd':
          app->flags |= APP_FLAG_BACKGROUND;
          break;
        case 'h':
        default:
          return -1;
        }
    }

  return 0;
}

int app_run(app *app)
{
  int e;

  e = capture_open(&app->capture, app_capture_event, &app, app->flags & APP_FLAG_MOCK_SDI ? CAPTURE_FLAG_MOCK : 0);
  if (e == -1)
    return -1;

  /*
  e = server_open(&app->audio_server, app_server_event, &app, app->audio_endpoint, app->sync);
  if (e == -1)
    return -1;

  e = server_open(&app->video_server, app_server_event, &app, app->video_endpoint, app->sync);
  if (e == -1)
    return -1;

  server_sync(&app->audio_server, &app->video_server);
  */

  if (app->flags & APP_FLAG_BACKGROUND)
    {
      e = daemon(0, 0);
      if (e == -1)
        err(1, "daemon");
    }

  return 0;
}
