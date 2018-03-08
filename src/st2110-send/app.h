#ifndef APP_H_INCLUDED
#define APP_H_INCLUDED

enum app_flag
{
  APP_FLAG_BACKGROUND = (1 << 0)
};

enum app_map_type
{
  APP_MAP_TYPE_AUDIO,
  APP_MAP_TYPE_VIDEO
};

typedef struct app_map app_map;
struct app_map
{
  int       type;
  char     *path;
  char     *node;
  char     *service;
  client    client;
  writer    writer;
};

typedef struct app app;
struct app
{
  int       flags;
  list      maps;
};

void app_usage(void);
int  app_construct(app *, int, char **);
int  app_run(app *);

#endif /* APP_H_INCLUDED */
