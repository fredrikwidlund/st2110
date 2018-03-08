#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netdb.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <dynamic.h>

#include "frame.h"
#include "packet.h"

static void packet_new_frame(packet *packet, size_t i)
{
  packet->frame[i] = frame_pool_new(&packet->pool);
  frame_reserve(packet->frame[i], 2048);
  packet->msg[i] = (struct mmsghdr) {.msg_hdr.msg_iov = &packet->frame[i]->memory, .msg_hdr.msg_iovlen = 1};
}

int packet_construct(packet *packet, struct addrinfo *addrinfo)
{
  int e, i;

  frame_pool_construct(&packet->pool, sizeof(frame));
  list_construct(&packet->queue);

  for (i = 0; i < IOV_MAX; i ++)
    {
      packet_new_frame(packet, i);
      packet->msg[i] = (struct mmsghdr) {
        .msg_hdr.msg_name = addrinfo->ai_addr,
        .msg_hdr.msg_namelen = addrinfo->ai_addrlen,
      };
    }

  packet->socket = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
  if (packet->socket == -1)
    return -1;

  e = bind(packet->socket, addrinfo->ai_addr, addrinfo->ai_addrlen);
  if (e == -1)
    {
      (void) close(packet->socket);
      packet->socket = -1;
      return -1;
    }

  (void) setsockopt(packet->socket, SOL_SOCKET, SO_RCVBUF, (int[]){64 * 1024 * 1024}, sizeof (int));
  return 0;
}

int packet_socket(packet *packet)
{
  return packet->socket;
}

int packet_input(packet *packet)
{
  int i, n;
  frame *f;

  n = recvmmsg(packet->socket, packet->msg, IOV_MAX, 0, NULL);
  if (n == -1)
    return errno == EAGAIN ? 0 : -1;

  for (i = 0; i < n; i ++)
    {
      f = packet->frame[i];
      f->data = (struct iovec){.iov_base = f->memory.iov_base, .iov_len = packet->msg[i].msg_len};
      list_push_back(&packet->queue, &f, sizeof f);
      packet_new_frame(packet, i);
    }

  return 0;
}

int packet_output(packet *packet)
{
  int i, n;
  frame **f;

  if (list_empty(&packet->queue))
    return 0;

  f = list_front(&packet->queue);
  for (i = 0; i < IOV_MAX && f != list_end(&packet->queue); i ++, f = list_next(f))
    {
      packet->msg[i].msg_hdr.msg_iov = &(*f)->data;
      packet->msg[i].msg_hdr.msg_iovlen = 1;
    }

  n = sendmmsg(packet->socket, packet->msg, i, 0);
  if (n == -1)
    return errno == EAGAIN ? 0 : -1;

  for (; n; n --)
    {
      f = list_front(&packet->queue);
      frame_release(*f);
      list_erase(f, NULL);
    }

  return 0;
}

frame *packet_read(packet *packet)
{
  frame **i, *f;

  if (list_empty(&packet->queue))
    return NULL;

  i = list_front(&packet->queue);
  f = *i;
  list_erase(i, NULL);
  return f;
}

void packet_write(packet *packet, frame *f)
{
  frame_hold(f);
  list_push_back(&packet->queue, &f, sizeof f);
}
