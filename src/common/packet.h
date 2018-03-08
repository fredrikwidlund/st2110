#ifndef PACKET_H_INCLUDED
#define PACKET_H_INCLUDED

typedef struct packet packet;
struct packet
{
  frame_pool      pool;
  list            queue;
  int             socket;
  frame          *frame[IOV_MAX];
  struct mmsghdr  msg[IOV_MAX];
};

int    packet_construct(packet *, struct addrinfo *);
int    packet_socket(packet *);
int    packet_input(packet *);
int    packet_output(packet *);
frame *packet_read(packet *);
void   packet_write(packet *, frame *);

#endif /* PACKET_H_INCLUDED */
