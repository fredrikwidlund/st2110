#ifndef RTP_H_INCLUDED
#define RTP_H_INCLUDED

typedef struct rtp_header rtp_header;
struct rtp_header
{
  unsigned   cc:4;
  unsigned   x:1;
  unsigned   p:1;
  unsigned   version:2;
  unsigned   pt:7;
  unsigned   m:1;
  uint16_t   sequence_number;
  uint32_t   timestamp;
  uint32_t   ssrc_identifier;
} __attribute__((packed));

typedef struct rtp rtp;
struct rtp
{
  list       queue;
  frame    **iterator;
  uint16_t   last;
};

void   rtp_construct(rtp *);
int    rtp_input(rtp *, frame *);
frame *rtp_read(rtp *);

#endif /* RTP_H_INCLUDED */
