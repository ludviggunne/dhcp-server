#include <stdlib.h>
#include <stdio.h>
// #include <assert.h>
#include <arpa/inet.h>

#include "addr_space.h"
#include "lease_queue.h"
#include "conf.h"

in_addr_t
parse_addr (const char *str)
{
  struct in_addr addr;
  inet_pton (AF_INET, str, &addr);
  return addr.s_addr;
}

void
print_addr (in_addr_t addr)
{
  struct in_addr in_addr = { .s_addr = addr };
  char buf[INET_ADDRSTRLEN];
  inet_ntop (AF_INET, &in_addr, buf, sizeof (buf));
  printf ("%s\n", buf);
}

int
main (int argc, char **argv)
{
  struct lease_queue lq;
  lq_init (&lq);

  struct lease lease;
  lease.in_addr = parse_addr ("192.168.0.12");
  lease.expire = 2;

  lq_add (&lq, &lease);

  lease.in_addr = parse_addr ("192.168.0.10");
  lease.expire = 0;

  lq_add (&lq, &lease);

  lease.in_addr = parse_addr ("192.168.0.15");
  lease.expire = 5;

  lq_add (&lq, &lease);

  lease.in_addr = parse_addr ("192.168.0.13");
  lease.expire = 3;

  lq_add (&lq, &lease);

  lease.in_addr = parse_addr ("192.168.0.11");
  lease.expire = 1;

  lq_add (&lq, &lease);

  lease.in_addr = parse_addr ("192.168.0.14");
  lease.expire = 4;

  lq_add (&lq, &lease);

  while (lq.nleases > 0) {
    struct lease *lease = lq_next (&lq);
    print_addr (lease->in_addr);
    printf ("%ld\n\n", lease->expire);
    lq_pop (&lq);
  }
}
