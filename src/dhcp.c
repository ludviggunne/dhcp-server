#include <string.h>

#include "dhcp.h"

static uint8_t dhcp_opt_magic_cookie[4] = {99, 130, 83, 99};

static int
dhcp_opt_iterator_add (dhcp_opt_iterator_t *it, const uint8_t *buf, uint8_t len)
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
dhcp_opt_iterator_take(dhcp_opt_iterator_t *it, uint8_t *buf, uint8_t len)
{
  if (len > it->left)
    return -1;

  memcpy(buf, it->opts, len);

  it->opts += len;
  it->left -= len;
  it->done = it->left == 0;

  return 0;
}

dhcp_opt_iterator_t
dhcp_opt_iterator_init(dhcp_msg_t *msg)
{
  return (dhcp_opt_iterator_t) {
    .opts = msg->options,
    .done = 0,
    .left = sizeof(msg->options),
  };
}

int
dhcp_opt_add_magic_cookie(dhcp_opt_iterator_t *it)
{
  return dhcp_opt_iterator_add(it, dhcp_opt_magic_cookie, sizeof(dhcp_opt_magic_cookie));
}

int
dhcp_opt_take_magic_cookie(dhcp_opt_iterator_t *it)
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
dhcp_opt_add_option(const dhcp_opt_t *opt, dhcp_opt_iterator_t *it)
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
dhcp_opt_take_option(dhcp_opt_t *opt, dhcp_opt_iterator_t *it)
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

