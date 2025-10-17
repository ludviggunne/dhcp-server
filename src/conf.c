#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include "conf.h"
#include "log.h"

static int
check_subnet_mask (in_addr_t addr)
{
  addr = ntohl (addr);

  while (addr & 0x80000000)
  {
    addr <<= 1;
  }

  return addr ? -1 : 0;
}

static int
parse_time (const char *str)
{
  int tot = 0;

  while (*str) {
    int x = 0;

    while (isdigit (*str)) {
      x *= 10;
      x += *str - '0';
      str++;
    }

    switch (*str) {
    case '\0':
      return -1;
    case 'h':
      x *= 3600;
      break;
    case 'm':
      x *= 60;
      break;
    case 's':
      break;
    default:
      return -1;
    }

    tot += x;
    str++;
  }

  return tot;
}

static void
strip_comment (char *line)
{
  for (char *ptr = line; *ptr; ptr++)
    if (*ptr == '#') {
      *ptr = '\0';
      return;
    }
}

int
conf_parse (const char *path, struct conf *conf)
{
  const char *const delims = " \t\n";
  int ret = 0;

  FILE *f = fopen (path, "r");
  if (f == NULL) {
    log_errno ("Failed to open configuration file %s", path);
    return -1;
  }

  char *line = NULL;
  size_t size;
  int lineno = 0;
  struct in_addr addr_buf;
  struct ether_addr *ether_ptr;

  while (getline (&line, &size, f) >= 0)
  {
    lineno++;

    strip_comment (line);
    char *option = strtok (line, delims);

    if (option == NULL)
      continue;

    if (strcmp (option, "interface") == 0) {
      char *name = strtok (NULL, delims);
      if (name == NULL) {
        log_error ("%s:%d: Missing interface name", path, lineno);
        ret = -1;
        goto done;
      }
      conf->interface = strdup (name);
      continue;
    }

    if (strcmp (option, "subnet-mask") == 0) {
      char *str = strtok (NULL, delims);
      if (str == NULL) {
        log_error ("%s:%d: Missing subnet mask", path, lineno);
        ret = -1;
        goto done;
      }

      if (inet_pton (AF_INET, str, &addr_buf) != 1
          || check_subnet_mask (addr_buf.s_addr) < 0) {
        log_error ("%s:%d: Invalid subnet mask: %s", path, lineno, str);
        ret = -1;
        goto done;
      }

      conf->subnet_mask = addr_buf.s_addr;
      continue;
    }

    if (strcmp (option, "lease-time") == 0) {
      char *str = strtok (NULL, delims);
      if (str == NULL) {
        log_error ("%s:%d: Missing lease time", path, lineno);
        ret = -1;
        goto done;
      }

      int time = parse_time (str);
      if (time < 0) {
        log_error ("%s:%d: Invalid lease time: %s", path, lineno, str);
        ret = -1;
        goto done;
      }

      conf->lease_time = time;
      continue;
    }

    if (strcmp (option, "request-window") == 0) {
      char *str = strtok (NULL, delims);
      if (str == NULL) {
        log_error ("%s:%d: Missing request window", path, lineno);
        ret = -1;
        goto done;
      }

      int time = parse_time (str);
      if (time < 0) {
        log_error ("%s:%d: Invalid request window: %s", path, lineno, str);
        ret = -1;
        goto done;
      }

      conf->request_window = time;
      continue;
    }

    if (strcmp (option, "range") == 0) {
      char *str = strtok (NULL, delims);
      if (str == NULL) {
        log_error ("%s:%d: Missing low address in range", path, lineno);
        ret = -1;
        goto done;
      }

      if (inet_pton (AF_INET, str, &addr_buf) != 1) {
        log_error ("%s:%d: Invalid high address in range: %s", path, lineno, str);
        ret = -1;
        goto done;
      }

      conf->range_lo = addr_buf.s_addr;

      str = strtok (NULL, delims);
      if (str == NULL) {
        log_error ("%s:%d: Missing high address in range", path, lineno);
        ret = -1;
        goto done;
      }

      if (inet_pton (AF_INET, str, &addr_buf) != 1) {
        log_error ("%s:%d: Invalid high address in range: %s", path, lineno, str);
        ret = -1;
        goto done;
      }

      conf->range_hi = addr_buf.s_addr;
      continue;
    }

    if (strcmp (option, "static") == 0) {
      conf->nstatic_confs++;
      conf->static_confs = realloc (conf->static_confs,
                                    sizeof (struct static_conf) * conf->nstatic_confs);
      struct static_conf *static_conf = &conf->static_confs[conf->nstatic_confs - 1];

      char *str = strtok (NULL, delims);
      if (str == NULL) {
        log_error ("%s:%d: Missing hardware address", path, lineno);
        ret = -1;
        goto done;
      }

      ether_ptr = ether_aton (str);

      if (ether_ptr == NULL) {
        log_error ("%s:%d: Invalid hardware address: %s", path, lineno, str);
        ret = -1;
        goto done;
      }

      memcpy (&static_conf->ether_addr, ether_ptr, sizeof (struct ether_addr));

      str = strtok (NULL, delims);
      if (str == NULL) {
        log_error ("%s:%d: Missing IPv4 address", path, lineno);
        ret = -1;
        goto done;
      }

      if (inet_pton (AF_INET, str, &addr_buf) != 1) {
        log_error ("%s:%d: Invalid IPv4 address: %s", path, lineno, str);
        ret = -1;
        goto done;
      }

      static_conf->in_addr = addr_buf.s_addr;

      str = strtok (NULL, delims);
      if (str == NULL) {
        /* Default lease time is 24h */
        static_conf->lease_time = 24 * 3600;
        continue;
      }

      int time = parse_time (str);
      if (time < 0) {
        log_error ("%s:%d: Invalid lease time: %s", path, lineno, str);
        ret = -1;
        goto done;
      }

      static_conf->lease_time = time;
      continue;
    }

    log_error ("%s:%d: Unknown configuration option '%s'", path, lineno, option);
    ret = -1;
    goto done;
  }

done:
  fclose (f);
  return ret;
}
