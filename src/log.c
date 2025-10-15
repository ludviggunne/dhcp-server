#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "log.h"

static __thread char *user_msg_buf = NULL;

static void
set_user_msg (const char *fmt, va_list ap)
{
  va_list ap_copy;
  size_t size;

  va_copy (ap_copy, ap);

  size = 1 + vsnprintf (NULL, 0, fmt, ap);
  user_msg_buf = realloc (user_msg_buf, size);
  va_end (ap);

  vsnprintf (user_msg_buf, size, fmt, ap_copy);
  va_end (ap_copy);
}

static const char *
timestamp (void)
{
  time_t t = time (NULL);
  char *ts = ctime (&t);

  for (char *p = ts; *p; p++)
    if (*p == '\n') {
      *p = '\0';
      break;
    }

  return ts;
}

void
log_info (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  set_user_msg (fmt, ap);

  const char *ts = timestamp ();
  fprintf (stdout, "%s [info] %s\n", ts, user_msg_buf);
}

void
log_error (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  set_user_msg (fmt, ap);

  const char *ts = timestamp ();
  fprintf (stderr, "%s \e[1m[error] %s\e[0m\n", ts, user_msg_buf);
}

void
log_errno (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  set_user_msg (fmt, ap);

  const char *ts = timestamp ();
  fprintf (stdout, "%s \e[1m[error] %s: %s\e[0m\n", ts, user_msg_buf, strerror (errno));
}

