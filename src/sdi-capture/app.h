#ifndef APP_H_INCLUDED
#define APP_H_INCLUDED

enum app_flag
{
  APP_FLAG_BACKGROUND = (1 << 0),
  APP_FLAG_MOCK_SDI   = (1 << 1)
};

typedef struct app app;
typedef struct app_map app_map;

struct app_map
{
  app     *app;
  char    *endpoint;
  server   server;
};

struct app
{
  int      flags;
  double   sync;
  char    *audio_endpoint;
  char    *video_endpoint;

  capture  capture;
  app_map  audio;
  app_map  video;
};

void app_usage(void);
int  app_construct(app *, int, char **);
void app_destruct(app *);
int  app_run(app *);

#endif /* APP_H_INCLUDED */
