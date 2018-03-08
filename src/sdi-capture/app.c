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
#include "server.h"
#include "app.h"

static int app_map_event(void *state, int type, void *data)
{
  app_map *map = state;

  switch (type)
    {
    case SERVER_EVENT_ERROR:
      err(1, "[%s] server", map->endpoint);
    case SERVER_EVENT_LOG:
      (void) fprintf(stderr, "%s\n", (char *) data);
      break;
    }

  return REACTOR_OK;
}

static int app_capture_event(void *state, int type, void *data)
{
  app *app = state;
  capture_frame *frame;

  if (type != CAPTURE_EVENT_FRAME)
    err(1, "capture error");

  frame = data;
  server_distribute(&app->audio.server, frame->audio);
  server_distribute(&app->video.server, frame->video);

  return REACTOR_OK;
}

static int app_map_construct(app_map *map, app *app, char *endpoint, double sync)
{
  *map = (app_map) {.app = app, .endpoint = endpoint};

  return server_open(&map->server, app_map_event, map, map->endpoint, sync);
}

static void app_map_sync(app_map *master, app_map *slave)
{
  server_sync(&master->server, &slave->server);
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

  e = capture_open(&app->capture, app_capture_event, app, app->flags & APP_FLAG_MOCK_SDI ? CAPTURE_FLAG_MOCK : 0);
  if (e == -1)
    return -1;

  e = app_map_construct(&app->audio, app, app->audio_endpoint, app->sync);
  if (e == -1)
    return -1;

  e = app_map_construct(&app->video, app, app->video_endpoint, app->sync);
  if (e == -1)
    return -1;

  app_map_sync(&app->audio, &app->video);

  if (app->flags & APP_FLAG_BACKGROUND)
    {
      e = daemon(0, 0);
      if (e == -1)
        err(1, "daemon");
    }

  return 0;
}

void app_destruct(app *app)
{
  capture_close(&app->capture);
  server_close(&app->audio.server);
  server_close(&app->video.server);
}
