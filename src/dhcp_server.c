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
}
