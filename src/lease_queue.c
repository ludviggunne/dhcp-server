#include <stdlib.h>
#include <string.h>

#include "lease_queue.h"

void
lq_init (struct lease_queue *lq)
{
  lq->leases = NULL;
  lq->nleases = 0;
  lq->capac = 0;
}

void
lq_deinit (struct lease_queue *lq)
{
  free (lq->leases);
}

int
lq_add (struct lease_queue *lq, struct lease *lease)
{
  if (lq->capac == 0) {
    lq->capac = 2;
    lq->leases = realloc (lq->leases, sizeof (*lq->leases) * lq->capac);
  }

  if (lq->nleases == lq->capac) {
    lq->capac *= 1.5f;
    lq->leases = realloc (lq->leases, sizeof (*lq->leases) * lq->capac);
  }

  memcpy (&lq->leases[lq->nleases], lease, sizeof (*lease));
  lq->nleases++;

  /* heapify-up */
  size_t ci = lq->nleases - 1;
  while (ci > 0)
  {
    size_t pi = (ci + 1) / 2 - 1;

    struct lease *child = &lq->leases[ci];
    struct lease *parent = &lq->leases[pi];

    if (child->expire < parent->expire) {
      struct lease tmp;

      memcpy (&tmp, child, sizeof (struct lease));
      memcpy (child, parent, sizeof (struct lease));
      memcpy (parent, &tmp, sizeof (struct lease));

      ci = pi;
      continue;
    }

    break;
  }

  return 0;
}

struct lease *
lq_next (struct lease_queue *lq)
{
  if (lq->nleases == 0)
    return NULL;

  return &lq->leases[0];
}

void
lq_pop (struct lease_queue *lq)
{
  lq_remove (lq, 0);
}

void
lq_remove (struct lease_queue *lq, size_t i)
{
  struct lease *leases = lq->leases;
  struct lease *last = &leases[lq->nleases - 1];
  struct lease *removed = &leases[i];

  memcpy (removed, last, sizeof (struct lease));
  lq->nleases--;

  /* heapify-down */
  size_t pi = i;
  while (pi < lq->nleases - 1)
  {
    struct lease *parent = &leases[pi];
    size_t mi = pi;
    size_t li = 2 * (pi + 1) - 1;
    size_t ri = li + 1;

    if (li <= lq->nleases && leases[li].expire < leases[mi].expire)
      mi = li;

    if (ri <= lq->nleases && leases[ri].expire < leases[mi].expire)
      mi = ri;

    if (mi == pi)
      return;

    struct lease tmp;
    struct lease *child = &leases[mi];

    memcpy (&tmp, child, sizeof (struct lease));
    memcpy (child, parent, sizeof (struct lease));
    memcpy (parent, &tmp, sizeof (struct lease));

    pi = mi;
  }
}
