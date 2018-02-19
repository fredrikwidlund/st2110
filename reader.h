#ifndef READER_H_INCLUDED
#define READER_H_INCLUDED

typedef struct reader_video_header reader_video_header;
struct reader_video_header
{
  uint16_t             extended_sequence_number;
  uint16_t             srd_length;
  uint16_t             srd_row_number;
  uint16_t             srd_offset;
} __attribute__((packed));

typedef struct reader_video reader_video;
struct reader_video
{
  int    width;
  int    height;
  int    pixel_group_size;
  int    pixel_group_count;
};

typedef struct reader_audio reader_audio;
struct reader_audio
{
  int    sample_size;
  int    sample_count;
};

typedef struct reader reader;
struct reader
{
  packet          packet;
  rtp             rtp;
  frame_pool      pool;
  frame          *frame;
  list            queue;
  union
  {
    reader_video  video;
    reader_audio  audio;
  };
};

int reader_construct(reader *, char *, char *, int, int, int, int);
int reader_socket(reader *);
int reader_read(reader *);

#endif /* READER_H_INLCUDED */
