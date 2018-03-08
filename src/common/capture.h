#ifndef CAPTURE_H_INCLUDED
#define CAPTURE_H_INCLUDED

enum capture_flag
{
  CAPTURE_FLAG_MOCK = (1 << 0)
};

enum capture_event
{
  CAPTURE_EVENT_ERROR,
  CAPTURE_EVENT_FRAME
};

typedef struct capture_frame capture_frame;
typedef struct capture capture;

struct capture
{
  reactor_user        user;
  reactor_descriptor  descriptor;
  int                 signal[2];
  list                frames;
  pthread_mutex_t     mutex;
  void               *internal;
};

struct capture_frame
{
  frame              *audio;
  frame              *video;
};

int  capture_open(capture *, reactor_user_callback *, void *, int);
void capture_close(capture *);

#endif /* CAPTURE_H_INCLUDED */
