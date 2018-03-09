#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <netdb.h>
#include <err.h>
#include <sys/uio.h>

#include <dynamic.h>
#include <reactor.h>

#include "frame.h"
#include "client.h"
#include "rtp.h"
#include "packet.h"
#include "reader.h"
#include "app.h"

static int app_map_parse_conf(app_map *map, char *conf)
{
  char *t;

  t = strsep(&conf, ":");
  map->path = t;
  t = strsep(&conf, ":");
  map->node = t;
  t = strsep(&conf, ":");
  map->service = t;
  if (!map->path || !map->node || !map->service)
    return -1;

  return 0;
}

static int app_map_input_event(void *state, int type, void *data)
{
  app_map *map = state;
  frame *f;
  int e;

  if (type != CLIENT_EVENT_FRAME)
    err(1, "app_map_input_event");
  f = data;

  e = writer_push(&map->writer, f);
  if (e == -1)
    err(1, "writer_push");

  return REACTOR_OK;
}

static int app_map_output_event(void *state, int type, void *data)
{
  app_map *map = state;

  if (type == WRITER_EVENT_ERROR)
    err(1, "app_map_output_event");

  printf("%p %d %p\n", (void *) map, type, data);
  return REACTOR_OK;
}

static int app_add_map(app *app, int type, char *conf)
{
  app_map *map;
  int e;

  //int e, st2110_type, payload_type;
  //size_t frame_size;

  map = calloc(1, sizeof *map);
  if (!map)
    abort();
  map->type = type;

  e = app_map_parse_conf(map, conf);
  if (e == -1)
    return -1;

  //st2110_type = type == APP_MAP_TYPE_AUDIO ? ST2110_TYPE_AUDIO : ST2110_TYPE_VIDEO;
  //payload_type = type == APP_MAP_TYPE_AUDIO ? 97 : 96;
  //frame_size = type == APP_MAP_TYPE_AUDIO ? 48000 * 2 * 2 / 50 : 1280 * 720 * 4 / 2;

  //e = client_open(&map->client, app_map_input_event, map, map->path, frame_size);

  list_push_back(&app->maps, &map, sizeof map);
  return 0;
}

static int app_map_run(app_map *map)
{
  int e;

  e = writer_open(&map->writer, app_map_output_event, map, map->node, map->service);
  if (e != REACTOR_OK)
    return -1;

  switch (map->type)
    {
    case APP_MAP_TYPE_AUDIO:
      writer_type_audio(&map->writer, 2, 2, 1800);
      return client_open(&map->client, app_map_input_event, map, map->path, 960 * 2 * 2);
    case APP_MAP_TYPE_VIDEO:
      writer_type_video(&map->writer, 1280, 720, 4, 2, 1800);
      return client_open(&map->client, app_map_input_event, map, map->path, 1280 * 720 * 4 / 2);
    }
  return -1;
}

void app_usage(void)
{
  extern char *__progname;

  (void) fprintf(stderr, "Usage: %s [OPTION]...\n", __progname);
  (void) fprintf(stderr, "SMPTE ST2110 sender\n");
  (void) fprintf(stderr, "\n");
  (void) fprintf(stderr, "Options:\n");
  (void) fprintf(stderr, "    -a PATH:HOST:PORT       add audio mapping from PATH to HOST:PORT\n");
  (void) fprintf(stderr, "    -v PATH:HOST:PORT       add video mapping from PATH to HOST:PORT\n");
  (void) fprintf(stderr, "    -d                      run in background\n");
  (void) fprintf(stderr, "    -h                      display this help\n");

  exit(EXIT_FAILURE);
}

int app_construct(app *app, int argc, char **argv)
{
   int c, e;

  *app = (struct app) {0};

  list_construct(&app->maps);

  while (1)
    {
      c = getopt(argc, argv, "a:v:s:mdh");
      if (c == -1)
        break;

      switch (c)
        {
        case 'a':
          e = app_add_map(app, APP_MAP_TYPE_AUDIO, optarg);
          if (e == -1)
            return -1;
          break;
        case 'v':
          e = app_add_map(app, APP_MAP_TYPE_VIDEO, optarg);
          if (e == -1)
            return -1;
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
  app_map **i;
  int e;

  list_foreach(&app->maps, i)
    {
      e = app_map_run(*i);
      if (e == -1)
        return -1;
    }

  return 0;
}
