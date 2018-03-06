#ifndef APP_H_INCLUDED
#define APP_H_INCLUDED

enum app_flag
{
  APP_FLAG_BACKGROUND = (1 << 0),
  APP_FLAG_MOCK_SDI   = (1 << 1)
};

typedef struct app app;
struct app
{
  int      flags;
  double   sync;

  capture  capture;

  char    *audio_endpoint;
  //server   audio_server;

  char    *video_endpoint;
  //server   video_server;
};

void app_usage(void);
int  app_construct(app *, int, char **);
int  app_run(app *);

#endif /* APP_H_INCLUDED */
