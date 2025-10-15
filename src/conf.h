#ifndef CONF_H_INCLUDED
#define CONF_H_INCLUDED

/* Configuration parsing */

#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ether.h>

/* Static configuration */
struct static_conf {
  /* IPv4 address */
  in_addr_t in_addr;

  /* Hardware address */
  struct ether_addr ether_addr;

  /* Lease time */
  time_t lease_time;
};

/* Parsed configuration */
struct conf {
  /* Static configurations */
  struct static_conf *static_confs;

  /* Number of static configurations */
  size_t nstatic_confs;

  /* Interface name */
  char *interface;

  /* Subnet mask */
  in_addr_t subnet_mask;

  /* Lease time */
  time_t lease_time;

  /* Address range, lowest address */
  in_addr_t range_lo;

  /* Address range, highest address */
  in_addr_t range_hi;
};

int conf_parse (const char *path, struct conf *conf);

#endif
