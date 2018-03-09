#ifndef PACKET_H_INCLUDED
#define PACKET_H_INCLUDED

enum packet_type
{
  PACKET_TYPE_READER,
  PACKET_TYPE_WRITER
};

typedef struct packet packet;
struct packet
{
  int              type;
  frame_pool       pool;
  list             queue;
  int              socket;
  frame          **frame;
  struct mmsghdr   msg[IOV_MAX];
};

int    packet_construct(packet *, int, struct addrinfo *);
int    packet_socket(packet *);
int    packet_input(packet *);
int    packet_output(packet *);
frame *packet_read(packet *);
void   packet_write(packet *, frame *);

#endif /* PACKET_H_INCLUDED */
