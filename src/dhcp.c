#include <string.h>

#include "dhcp.h"

static uint8_t magic_cookie[4] = {99, 130, 83, 99};

static int
dhcp_oit_add (struct dhcp_oit *it, const uint8_t *buf, uint8_t len)
{
  if (len > it->left)
    return -1;

  memcpy (it->opts, buf, len);

  it->opts += len;
  it->left -= len;
  it->done = it->left == 0;

  return 0;
}

static int
dhcp_oit_take (struct dhcp_oit *it, uint8_t *buf, uint8_t len)
{
  if (len > it->left)
    return -1;

  memcpy (buf, it->opts, len);

  it->opts += len;
  it->left -= len;

  return 0;
}

struct dhcp_oit
dhcp_oit_init (struct dhcp_msg *msg)
{
  return (struct dhcp_oit) {
    .opts = msg->options,
    .done = 0,
    .left = sizeof (msg->options),
  };
}

int
dhcp_add_magic_cookie (struct dhcp_oit *it)
{
  return dhcp_oit_add (it, magic_cookie, sizeof (magic_cookie));
}

int
dhcp_eat_magic_cookie (struct dhcp_oit *it)
{
  uint8_t buf[sizeof (magic_cookie)];

  if (dhcp_oit_take (it, buf, sizeof (buf)) < 0)
    return -1;

  for (size_t i = 0; i < sizeof (buf); i++)
    if (buf[i] != magic_cookie[i])
      return -1;

  return 0;
}

int
dhcp_opt_add (const struct dhcp_opt *opt, struct dhcp_oit *it)
{
  if (dhcp_oit_add (it, &opt->tag, 1) < 0)
    return -1;

  if (opt->tag == DHCP_OPT_PAD_OPTION)
    return 0;

  if (opt->tag == DHCP_OPT_END_OPTION) {
    memset (it->opts, 0, it->left);
    return 0;
  }

  if (dhcp_oit_add (it, &opt->len, 1) < 0)
    return -1;

  return dhcp_oit_add (it, opt->buf, opt->len);
}

int
dhcp_opt_take (struct dhcp_opt *opt, struct dhcp_oit *it)
{
  for (;;) {
    if (dhcp_oit_take (it, &opt->tag, 1) < 0)
      return -1;

    if (opt->tag != DHCP_OPT_PAD_OPTION)
      break;
  }

  if (opt->tag == DHCP_OPT_END_OPTION) {
    it->done = 1;
    return 0;
  }

  if (dhcp_oit_take (it, &opt->len, 1) < 0)
    return -1;

  if (opt->len > it->left)
    return -1;

  return dhcp_oit_take (it, opt->buf, opt->len);
}

const char *
dhcp_msg_type_str (uint8_t type)
{
  switch (type) {
  case DHCP_MSG_TYPE_DHCPDISCOVER: return "DHCPDISCOVER";
  case DHCP_MSG_TYPE_DHCPOFFER: return "DHCPOFFER";
  case DHCP_MSG_TYPE_DHCPREQUEST: return "DHCPREQUEST";
  case DHCP_MSG_TYPE_DHCPDECLINE: return "DHCPDECLINE";
  case DHCP_MSG_TYPE_DHCPACK: return "DHCPACK";
  case DHCP_MSG_TYPE_DHCPNAK: return "DHCPNAK";
  case DHCP_MSG_TYPE_DHCPRELEASE: return "DHCPRELEASE";
  default: return "???";
  }
}
