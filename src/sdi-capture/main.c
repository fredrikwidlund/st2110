#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <err.h>
#include <netdb.h>

#include <dynamic.h>
#include <reactor.h>

#include "frame.h"
#include "capture.h"
#include "app.h"

//#include "server.h"

int main(int argc, char **argv)
{
  app app;
  int e;

  signal(SIGPIPE, SIG_IGN);

  e = app_construct(&app, argc, argv);
  if (e == -1)
    app_usage();

  reactor_core_construct();

  e = app_run(&app);
  if (e == -1)
    err(1, "app_run");

  e = reactor_core_run();
  if (e == -1)
    err(1, "reactor_core_run");

  reactor_core_destruct();
}
