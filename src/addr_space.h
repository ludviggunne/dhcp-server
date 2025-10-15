#ifndef ADDR_ALLOC_H_INCLUDED
#define ADDR_ALLOC_H_INCLUDED

/* Address allocation */

#include <netinet/in.h>

struct addr_space {
  /* Lowest address that may be allocated */
  in_addr_t lo;

  /* Highest address that may be allocated */
  in_addr_t hi;

  /* The next address to be allocated if free list is empty */
  in_addr_t next;

  /* List of free addresses */
  in_addr_t *free;

  /* Length of free list */
  size_t nfree;
};

/* Initialize address space */
void as_init (struct addr_space *as, in_addr_t lo, in_addr_t hi);

/* Allocate a new address */
int as_alloc (struct addr_space *as, in_addr_t *addr);

/* Free an allocated address */
void as_free (struct addr_space *as, in_addr_t addr);

/* Dispose of address space */
void as_deinit (struct addr_space *as);

#endif
