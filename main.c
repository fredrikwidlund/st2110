#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <limits.h>
#include <fcntl.h>
#include <poll.h>
#include <err.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <dynamic.h>

#include "frame.h"
#include "packet.h"
#include "rtp.h"
#include "reader.h"

int main(int argc, char **argv)
{
  reader video; //, audio;
  struct pollfd fds[2];
  int e;

  e = reader_construct(&video, argv[1], argv[2], 1280, 720, 4, 2);
  if (e == -1)
    err(1, "reader_construct video");

  //e = reader_construct(&video, argv[1], argv[3]);
  //if (e == -1)
  //  err(1, "reader_construct video");

  fds[0].fd = reader_socket(&video);
  fds[0].events = POLLIN;
  //fds[1].fd = reader_socket(&video);
  //fds[1].events = POLLIN;

  while (1)
    {
      e = poll(fds, 1, -1);
      if (e == -1)
        err(1, "poll");

      if (fds[0].revents & POLLIN)
        {
          e = reader_read(&video);
          if (e == -1)
            err(1, "reader_read video");
        }

      //if (fds[1].revents & POLLIN)
      //   {
      //    e = reader_read(&video);
      //    if (e == -1)
      //      err(1, "reader_read video");
      //  }
    }
}
