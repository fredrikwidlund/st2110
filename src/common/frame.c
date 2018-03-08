#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/uio.h>

#include <dynamic.h>

#include "frame.h"

static void frame_pool_release(void *object)
{
  frame_free(*(frame **) object);
}

void frame_pool_construct(frame_pool *pool, size_t size)
{
  vector_construct(&pool->frames, sizeof (frame *));
  pool->frame_size = size;
}

void frame_pool_destruct(frame_pool *pool)
{
  vector_destruct(&pool->frames, frame_pool_release);
}

frame *frame_pool_new(frame_pool *pool)
{
  frame *frame;

  if (vector_empty(&pool->frames))
    {
      frame = frame_new(pool->frame_size);
      frame->pool = pool;
      return frame;
    }

  frame = *(struct frame **) vector_back(&pool->frames);
  frame_hold(frame);
  vector_pop_back(&pool->frames, NULL);
  return frame;
}

frame *frame_new(size_t size)
{
  frame *frame;

  frame = calloc(1, size);
  if (!frame)
    abort();
  frame_hold(frame);
  return frame;
}

void frame_free(frame *frame)
{
  free(frame->memory.iov_base);
  free(frame);
}

void frame_reserve(frame *frame, size_t size)
{
  if (size != frame->memory.iov_len)
    {
      frame->memory.iov_base = realloc(frame->memory.iov_base, size);
      if (!frame->memory.iov_base)
        abort();
      frame->memory.iov_len = size;
      frame->data.iov_base = frame->memory.iov_base;
    }
}

void frame_hold(frame *frame)
{
  frame->ref ++;
}

void frame_release(frame *frame)
{
  frame->ref --;
  if (!frame->ref)
    {
      if (frame->pool)
        vector_push_back(&frame->pool->frames, &frame);
      else
        frame_free(frame);
    }
}

void *frame_data(frame *f)
{
  return f->data.iov_base;
}

size_t frame_size(frame *f)
{
  return f->data.iov_len;
}
