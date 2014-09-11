#include "dht.h"

#include <iobuf.h>
#include <errno.h>

#define BUFLEN 512

static char *
dht_gen_transaction_id(void)
{
  char *tid = malloc(3);

  snprintf(tid, 3, "%x%x", (uint8_t)rand_between(0, 15),
           (uint8_t)rand_between(0, 15));
  return tid;
}

void
dht_query_ping(struct peer *p)
{
  char *tid = dht_gen_transaction_id();
  struct iobuf iob = iobuf_init(56);
  struct sockaddr_storage dht_addr = p->addr;
  struct sockaddr_in *ipv4addr;
  struct sockaddr_in6 *ipv6addr;
  int ret = 0;
  int sd;
  uint8_t dht_id[20];
  char buf[BUFLEN];
  uint8_t *peer_id = (uint8_t*)btpd_get_peer_id();

  switch (dht_addr.ss_family) {
  case AF_INET:
    ipv4addr = (struct sockaddr_in *)&dht_addr;
    ipv4addr->sin_port = htons(p->dht_port);
    break;
  case AF_INET6:
    ipv6addr = (struct sockaddr_in6 *)&dht_addr;
    ipv6addr->sin6_port = htons(p->dht_port);
    break;
  default:
    return;
  }

  SHA1(peer_id, 20, dht_id);

  btpd_log(BTPD_L_MSG, "Sending dht query 'ping' to (%p:%d) "
      "[pid=%s; tid=%s; pidsize=%lu]\n",
      p, p->dht_port, peer_id, tid, sizeof(peer_id));

  iobuf_write(&iob, "d1:t2:", 6);
  iobuf_write(&iob, tid, 2);
  iobuf_write(&iob, "1:y1:q1:q4:ping1:ad2:id20:", 26);
  iobuf_write(&iob, dht_id, 20);
  iobuf_write(&iob, "ee", 2);

  btpd_log(BTPD_L_MSG, "query_msg='%s'; size=%lu\n", iob.buf, iob.size);
  if ((sd = socket(dht_addr.ss_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    btpd_log(BTPD_L_ERROR, "failed to open socket\n");

  ret = sendto(sd, iob.buf, iob.size, 0, (struct sockaddr*)&dht_addr,
               p->addrlen);
  btpd_log(BTPD_L_MSG, "sent %d bytes; err=%s\n", ret, strerror(errno));
  if (ret != -1) {
    memset(buf, 0, BUFLEN);
    ret = recvfrom(sd, buf, BUFLEN, 0, (struct sockaddr*)&dht_addr,
                   &p->addrlen);
    btpd_log(BTPD_L_MSG, "received=%d; response_msg=%s\n", ret, buf);
  }

  iobuf_free(&iob);
  free(tid);
  close(sd);
}
