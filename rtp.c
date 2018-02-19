#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

#include <dynamic.h>

#include "frame.h"
#include "rtp.h"

static int16_t rtp_distance(frame *a, frame *b)
{
  return
    ntohs(((rtp_header *) frame_data(b))->sequence_number) -
    ntohs(((rtp_header *) frame_data(a))->sequence_number);
}

static void rtp_flush(rtp *rtp)
{
  frame **i;

  while (!list_empty(&rtp->queue))
    {
      i = list_front(&rtp->queue);
      if (rtp_distance(*i, *rtp->iterator) <= 0)
        break;
      frame_release(*i);
      list_erase(i, NULL);
    }
}

void rtp_construct(rtp *rtp)
{
  list_construct(&rtp->queue);
  rtp->iterator = NULL;
}

int rtp_input(rtp *rtp, frame *f)
{
  frame **i;
  uint16_t seq;

  if (frame_size(f) < sizeof(rtp_header))
    return -1;

  seq = ntohs(((rtp_header *) frame_data(f))->sequence_number);
  if (seq != (uint16_t) (rtp->last + 1))
    fprintf(stderr, "lost %u %u\n", rtp->last, seq);
  rtp->last = seq;

  list_foreach_reverse(&rtp->queue, i)
    if (rtp_distance(*i, f) >= 0)
        break;

  frame_hold(f);
  list_insert(list_next(i), &f, sizeof f);
  return 0;
}

frame *rtp_read(rtp *rtp)
{
  frame **i;

  if (list_empty(&rtp->queue))
    return NULL;

  if (rtp->iterator)
    {
      i = list_next(rtp->iterator);
      if (i == list_end(&rtp->queue) || rtp_distance(*rtp->iterator, *i) != 1)
        return NULL;
      rtp->iterator = i;
    }
  else
    rtp->iterator = list_front(&rtp->queue);

  rtp_flush(rtp);
  frame_hold(*rtp->iterator);
  return *rtp->iterator;
}
