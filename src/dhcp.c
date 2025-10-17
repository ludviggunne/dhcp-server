#include <string.h>

#include "dhcp.h"

static uint8_t dhcp_opt_magic_cookie[4] = {99, 130, 83, 99};

static int
dhcp_opt_iterator_add (struct dhcp_opt_iterator *it, const uint8_t *buf, uint8_t len)
{
  if (len > it->left)
    return -1;

  memcpy(it->opts, buf, len);

  it->opts += len;
  it->left -= len;
  it->done = it->left == 0;

  return 0;
}

static int
dhcp_opt_iterator_take(struct dhcp_opt_iterator *it, uint8_t *buf, uint8_t len)
{
  if (len > it->left)
    return -1;

  memcpy(buf, it->opts, len);

  it->opts += len;
  it->left -= len;
  it->done = it->left == 0;

  return 0;
}

struct dhcp_opt_iterator
dhcp_opt_iterator_init(struct dhcp_msg *msg)
{
  return (struct dhcp_opt_iterator) {
    .opts = msg->options,
    .done = 0,
    .left = sizeof(msg->options),
  };
}

int
dhcp_opt_add_magic_cookie(struct dhcp_opt_iterator *it)
{
  return dhcp_opt_iterator_add(it, dhcp_opt_magic_cookie, sizeof(dhcp_opt_magic_cookie));
}

int
dhcp_opt_take_magic_cookie(struct dhcp_opt_iterator *it)
{
  uint8_t buf[sizeof(dhcp_opt_magic_cookie)];

  if (dhcp_opt_iterator_take(it, buf, sizeof(buf)) < 0)
    return -1;

  for (size_t i = 0; i < sizeof(buf); i++)
    if (buf[i] != dhcp_opt_magic_cookie[i])
      return -1;

  return 0;
}

int
dhcp_opt_add_option(const struct dhcp_opt *opt, struct dhcp_opt_iterator *it)
{
  if (dhcp_opt_iterator_add(it, &opt->tag, 1) < 0)
    return -1;

  if (opt->tag == DHCP_OPT_PAD_OPTION || opt->tag == DHCP_OPT_END_OPTION)
    return 0;

  if (dhcp_opt_iterator_add(it, &opt->len, 1) < 0)
    return -1;

  return dhcp_opt_iterator_add(it, opt->buf, opt->len);
}

int
dhcp_opt_take_option(struct dhcp_opt *opt, struct dhcp_opt_iterator *it)
{
  for (;;) {
    if (dhcp_opt_iterator_take(it, &opt->tag, 1) < 0)
      return -1;

    if (opt->tag != DHCP_OPT_PAD_OPTION)
      break;
  }

  if (dhcp_opt_iterator_take(it, &opt->len, 1) < 0)
    return -1;

  // TODO
}

