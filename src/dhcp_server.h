#ifndef DHCP_SERVER_H_INCLUDED
#define DHCP_SERVER_H_INCLUDED

#include "addr_space.h"
#include "lease_queue.h"
#include "conf.h"

extern struct conf g_conf;
extern struct lease_queue g_leaseq;
extern struct addr_space g_aspace;
extern int g_sockfd;

#endif
