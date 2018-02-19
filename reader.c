#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <netdb.h>
#include <limits.h>
#include <fcntl.h>

#include <dynamic.h>

#include "frame.h"
#include "packet.h"
#include "rtp.h"
#include "reader.h"

static struct addrinfo *resolve(char *node, char *service)
{
  struct addrinfo hints = {.ai_flags = AI_PASSIVE, .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
  struct addrinfo *addrinfo = NULL;
  int e;

  e = getaddrinfo(node, service, &hints, &addrinfo);
  return e == -1 ? NULL : addrinfo;
}

static int reader_input(reader *reader, frame *f)
{
  rtp_header *h;
  reader_video_header *v;

  h = frame_data(f);
  v = (reader_video_header *) (h + 1);

  if (ntohs(v->srd_row_number) == 0 && ntohs(v->srd_offset) == 0)
    {
      //if (!reader->frame)
      //  return -1;
      reader->frame = frame_pool_new(&reader->pool);
      frame_reserve(reader->frame, reader->video.width * reader->video.height *
                    reader->video.pixel_group_size / reader->video.pixel_group_count);
      reader->frame->data = reader->frame->memory;
    }

  if (!reader->frame)
     return 0;

   memcpy((uint8_t *) frame_data(reader->frame) +
          (((ntohs(v->srd_row_number) * reader->video.width) + ntohs(v->srd_offset)) *
           reader->video.pixel_group_size / reader->video.pixel_group_count),
          v + 1, ntohs(v->srd_length));

   if (h->m)
    {
      printf("frame\n");
      list_push_back(&reader->queue, &reader->frame, sizeof reader->frame);
      reader->frame = NULL;
    }

   return 0;
}

int reader_construct(reader *reader, char *node, char *service,
                     int width, int height, int pixel_group_size, int pixel_group_count)
{
  struct addrinfo *addrinfo;
  int e;

  *reader = (struct reader) {
    .video.width = width,
    .video.height = height,
    .video.pixel_group_size = pixel_group_size,
    .video.pixel_group_count = pixel_group_count
  };

  rtp_construct(&reader->rtp);
  frame_pool_construct(&reader->pool, sizeof(frame));
  list_construct(&reader->queue);

  addrinfo = resolve(node, service);
  if (!addrinfo)
    return -1;

  e = packet_construct(&reader->packet, addrinfo);
  freeaddrinfo(addrinfo);
  if (e == -1)
    return -1;
  (void) fcntl(packet_socket(&reader->packet), F_SETFL, O_NONBLOCK);

  return 0;
}

int reader_socket(reader *reader)
{
  return packet_socket(&reader->packet);
}

int reader_read(reader *reader)
{
  frame *f;
  int e;

  e = packet_input(&reader->packet);
  if (e == -1)
    return -1;

  while (1)
    {
      f = packet_read(&reader->packet);
      if (!f)
        break;

      e = rtp_input(&reader->rtp, f);
      frame_release(f);
      if (e == -1)
        return -1;

      while (1)
        {
          f = rtp_read(&reader->rtp);
          if (!f)
            break;
          e = reader_input(reader, f);
          frame_release(f);
          if (e == -1)
            return -1;
        }
    }

  return 0;
}
