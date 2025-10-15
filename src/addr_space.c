#include <stdlib.h>
#include <assert.h>

#include "addr_space.h"

void
as_init (struct addr_space *as, in_addr_t lo, in_addr_t hi)
{
  assert (ntohl (lo) <= ntohl (hi));
  assert (lo != 0);
  assert (hi != (in_addr_t) -1);

  as->hi = hi;
  as->lo = lo;
  as->next = lo;
  as->free = NULL;
  as->nfree = 0;
}

int
as_alloc (struct addr_space *as, in_addr_t *addr)
{
  if (as->nfree > 0) {
    *addr = as->free[as->nfree - 1];
    as->nfree--;
    return 0;
  }

  if (ntohl (as->next) > ntohl (as->hi))
    return -1;

  *addr = as->next;
  as->next = htonl (ntohl (as->next) + 1);

  return 0;
}

void
as_free (struct addr_space *as, in_addr_t addr)
{
  if (ntohl (addr) == ntohl (as->next) - 1) {
    as->next = htonl (ntohl (as->next) - 1);
    return;
  }

  as->nfree++;
  as->free = realloc (as->free, sizeof (*as->free) * as->nfree);

  as->free[as->nfree - 1] = addr;
}

void
as_deinit (struct addr_space *as)
{
  free (as->free);
}
