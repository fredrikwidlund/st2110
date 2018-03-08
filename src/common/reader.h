#ifndef READER_H_INCLUDED
#define READER_H_INCLUDED

enum reader_type
{
  READER_TYPE_UNDEFINED = 0,
  READER_TYPE_AUDIO,
  READER_TYPE_VIDEO
};

enum reader_event
{
  READER_EVENT_ERROR,
  READER_EVENT_DATA
};

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
  int    duration;
  int    width;
  int    height;
  int    pixel_group_size;
  int    pixel_group_count;
};

typedef struct reader_audio reader_audio;
struct reader_audio
{
  int    duration;
  int    sample_size;
  int    sample_channels;
};

typedef struct reader_frame reader_frame;
struct reader_frame
{
  frame    frame;
  uint64_t time;
  uint64_t duration;
};

typedef struct reader reader;
struct reader
{
  reactor_user        user;
  reactor_descriptor  descriptor;
  packet              packet;
  rtp                 rtp;
  frame_pool          pool;
  reader_frame       *frame;
  list                queue;
  size_t              queue_size;
  int                 type;
  union
  {
    reader_video      video;
    reader_audio      audio;
  };
};

enum writer_type
{
  WRITER_TYPE_UNDEFINED = 0,
  WRITER_TYPE_AUDIO,
  WRITER_TYPE_VIDEO
};

enum writer_event
{
  WRITER_EVENT_ERROR
};

typedef struct writer writer;
struct writer
{
  int                 type;
  reactor_user        user;
  reactor_descriptor  descriptor;
  packet              packet;
  rtp                 rtp;
  frame_pool          pool;
  list                queue;
  size_t              queue_size;
  union
  {
    reader_video      video;
    reader_audio      audio;
  };
};

int           reader_open(reader *, reactor_user_callback *, void *, char *, char *);
void          reader_type_audio(reader *, int, int, int);
void          reader_type_video(reader *, int, int, int, int, int);
reader_frame *reader_front(reader *);
void          reader_pop(reader *);

int           writer_open(writer *,reactor_user_callback *, void *, char *, char *);
void          writer_type_audio(writer *, int, int, int);
void          writer_type_video(writer *, int, int, int, int, int);
int           writer_push(writer *, frame *);

#endif /* READER_H_INLCUDED */
