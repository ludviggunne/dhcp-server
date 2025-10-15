#ifndef LEASE_H_INCLUDED
#define LEASE_H_INCLUDED

/* Lease handling */

#include <time.h>
#include <netinet/in.h>
#include <netinet/ether.h>

struct lease {
  /* Assigned IPv4 address */
  in_addr_t in_addr;

  /* Hardware address */
  struct ether_addr ether_addr;

  /* Expiration time */
  time_t expire;
};

struct lease_queue {
  /* Set of active leases */
  struct lease *leases;

  /* Number of active leases */
  size_t nleases;

  /* Allocation size */
  size_t capac;
};

/* Initialize lease set */
void lq_init (struct lease_queue *lq);

/* Deinitialize lease set */
void lq_deinit (struct lease_queue *lq);

/* Add a new lease */
int lq_add (struct lease_queue *lq, struct lease *lease);

/* Get the lease that will expire next */
struct lease *lq_next (struct lease_queue *lq);

/* Remove the lease that will expire next */
void lq_pop (struct lease_queue *lq);

/* Remove lease at specified index */
void lq_remove (struct lease_queue *lq, size_t i);

#endif
