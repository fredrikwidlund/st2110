#ifndef FRAME_H_INCLUDED
#define FRAME_H_INCLUDED

typedef struct frame_pool frame_pool;
struct frame_pool
{
  vector        frames;
  size_t        frame_size;
};

typedef struct frame frame;
struct frame
{
  size_t        ref;
  frame_pool   *pool;
  struct iovec  data;
  struct iovec  memory;
};

void    frame_pool_construct(frame_pool *, size_t);
void    frame_pool_destruct(frame_pool *);
frame  *frame_pool_new(frame_pool *);

frame  *frame_new(size_t);
void    frame_free(frame *);
void    frame_reserve(frame *, size_t);
void    frame_hold(frame *);
void    frame_release(frame *);
void   *frame_data(frame *);
size_t  frame_size(frame *);

#endif /* FRAME_H_INCLUDED */
