#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <netdb.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include <dynamic.h>
#include <reactor.h>

#include "frame.h"
#include "packet.h"
#include "rtp.h"
#include "reader.h"

static int reader_error(reader *reader)
{
  return reactor_user_dispatch(&reader->user, READER_EVENT_ERROR, NULL);
}

static struct addrinfo *resolve(char *node, char *service)
{
  struct addrinfo hints = {.ai_flags = AI_PASSIVE, .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
  struct addrinfo *addrinfo = NULL;
  int e;

  e = getaddrinfo(node, service, &hints, &addrinfo);
  return e == -1 ? NULL : addrinfo;
}

static int reader_audio_input(reader *reader, frame *f)
{
  rtp_header *h;
  size_t offset, size;

  h = frame_data(f);

  offset = (ntohl(h->timestamp) % reader->audio.duration)
    * reader->audio.sample_size * reader->audio.sample_channels * 48000 / 90000;
  if (!offset)
    {
      reader->frame = (reader_frame *) frame_pool_new(&reader->pool);
      reader->frame->time = ntohl(h->timestamp);
      reader->frame->duration = reader->audio.duration;
      frame_reserve(&reader->frame->frame, reader->audio.duration * reader->audio.sample_size * reader->audio.sample_channels
                    * 48000 / 90000);
      reader->frame->frame.data = reader->frame->frame.memory;
    }

  if (!reader->frame)
    return 0;

  size = frame_size(f) - sizeof *h;
  memcpy((uint8_t *) frame_data(&reader->frame->frame) + offset, h + 1, size);
  if (offset + size == frame_size(&reader->frame->frame))
    {
      list_push_back(&reader->queue, &reader->frame, sizeof reader->frame);
      reader->queue_size ++;
      reader->frame = NULL;
    }

  return 0;
}

static int reader_video_input(reader *reader, frame *f)
{
  rtp_header *h;
  reader_video_header *v;

  h = frame_data(f);
  v = (reader_video_header *) (h + 1);

  if (ntohs(v->srd_row_number) == 0 && ntohs(v->srd_offset) == 0)
    {
      reader->frame = (reader_frame *) frame_pool_new(&reader->pool);
      reader->frame->time = ntohl(h->timestamp);
      reader->frame->duration = reader->video.duration;
      frame_reserve(&reader->frame->frame, reader->video.width * reader->video.height *
                    reader->video.pixel_group_size / reader->video.pixel_group_count);
      reader->frame->frame.data = reader->frame->frame.memory;
    }

  if (!reader->frame)
     return 0;

  memcpy((uint8_t *) frame_data(&reader->frame->frame) +
          (((ntohs(v->srd_row_number) * reader->video.width) + ntohs(v->srd_offset)) *
           reader->video.pixel_group_size / reader->video.pixel_group_count),
          v + 1, ntohs(v->srd_length));

   if (h->m)
    {
      list_push_back(&reader->queue, &reader->frame, sizeof reader->frame);
      reader->queue_size ++;
      reader->frame = NULL;
    }

   return 0;
}

static int reader_input(reader *reader, frame *f)
{
  switch (reader->type)
    {
    case READER_TYPE_AUDIO:
      return reader_audio_input(reader, f);
    case READER_TYPE_VIDEO:
      return reader_video_input(reader, f);
    }
  return 0;
}

static int reader_read(reader *reader)
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

static int reader_event(void *state, int type, void *data)
{
  reader *reader = state;
  int e, flags = *(int *) data;
  size_t size;

  (void) type;
  if (flags & EPOLLIN)
    {
      size = reader->queue_size;
      e = reader_read(reader);
      if (e == -1)
        return reader_error(reader);

      if (size != reader->queue_size)
        {
          e = reactor_user_dispatch(&reader->user, READER_EVENT_DATA, NULL);
          if (e == REACTOR_ABORT)
            return REACTOR_ABORT;
        }
      flags &= ~EPOLLIN;
    }

  if (flags)
    return reader_error(reader);
  return REACTOR_OK;
}

int reader_open(reader *reader, reactor_user_callback *callback, void *state, char *node, char *service)
{
  struct addrinfo *addrinfo;
  int e;

  *reader = (struct reader) {.type = READER_TYPE_UNDEFINED};
  reactor_user_construct(&reader->user, callback, state);
  rtp_construct(&reader->rtp);
  frame_pool_construct(&reader->pool, sizeof(reader_frame));
  list_construct(&reader->queue);

  addrinfo = resolve(node, service);
  if (!addrinfo)
    return -1;

  e = packet_construct(&reader->packet, addrinfo);
  freeaddrinfo(addrinfo);
  if (e == -1)
    return -1;

  return reactor_descriptor_open(&reader->descriptor, reader_event, reader, packet_socket(&reader->packet), EPOLLIN);
}

void reader_type_audio(reader *reader, int sample_size, int sample_channels, int duration)
{
  reader->type = READER_TYPE_AUDIO;
  reader->audio = (reader_audio) {
    .duration = duration,
    .sample_size = sample_size,
    .sample_channels = sample_channels
  };
}

void reader_type_video(reader *reader, int width, int height, int pixel_group_size, int pixel_group_count, int duration)
{
  reader->type = READER_TYPE_VIDEO;
  reader->video = (reader_video) {
    .duration = duration,
    .width = width,
    .height = height,
    .pixel_group_size = pixel_group_size,
    .pixel_group_count = pixel_group_count
  };
}

reader_frame *reader_front(reader *reader)
{
  if (list_empty(&reader->queue))
    return NULL;
  return *(reader_frame **) list_front(&reader->queue);
}

void reader_pop(reader *reader)
{
  reader_frame **i;

  if (list_empty(&reader->queue))
    return;

  i = list_front(&reader->queue);
  frame_release(&(*i)->frame);
  list_erase(i, NULL);
  reader->queue_size --;
}
