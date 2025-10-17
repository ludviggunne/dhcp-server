#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <unistd.h>
#include <ifaddrs.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "dhcp.h"
#include "dhcp_server.h"
#include "log.h"

struct conf g_conf;
struct lease_queue g_leaseq;
struct addr_space g_aspace;
int g_sockfd;
in_addr_t g_server_addr;
char g_hostname[HOST_NAME_MAX];

static const int poll_timeout = 1 * 1000;
static struct sockaddr_in client_addr;

static int get_servaddr (void);
static char *inet_str (in_addr_t in_addr);
static void process_discover (struct dhcp_msg *msg);
static void process_request (struct dhcp_msg *msg);
static void process_release (struct dhcp_msg *msg);

int
main (int argc, char **argv)
{
  const char *conf_path = "./dhcp-server.conf";
  struct pollfd pollfd;
  struct sockaddr_in sockaddr;
  int en_broadcast = 1;

  if (argc > 1)
    conf_path = argv[1];

  if (conf_parse (conf_path, &g_conf) < 0)
    exit (EXIT_FAILURE);

  if (get_servaddr () < 0)
    exit (EXIT_FAILURE);

  if (gethostname (g_hostname, sizeof (g_hostname))) {
    log_errno ("gethostname()");
    exit (EXIT_FAILURE);
  }

  as_init (&g_aspace, g_conf.range_lo, g_conf.range_hi);
  lq_init (&g_leaseq);

  if ((g_sockfd = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
    log_errno ("Failed to open socket");
    exit (EXIT_FAILURE);
  }

  pollfd.fd = g_sockfd;
  pollfd.events = POLLIN;

  /* Bind socket to configured device */
  if (setsockopt (g_sockfd, SOL_SOCKET, SO_BINDTODEVICE,
                  g_conf.interface, strlen (g_conf.interface)) < 0) {
    log_errno ("Failed to bind socket to device %s", g_conf.interface);
    exit (EXIT_FAILURE);
  }

  /* Enabled broadcasting */
  if (setsockopt (g_sockfd, SOL_SOCKET, SO_BROADCAST,
                  &en_broadcast, sizeof (en_broadcast)) < 0) {
    log_errno ("Failed to enable broadcasting");
    exit (EXIT_FAILURE);
  }

  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons (DHCP_PORT_SERVER);
  sockaddr.sin_addr.s_addr = INADDR_ANY;

  client_addr.sin_addr.s_addr = INADDR_BROADCAST;
  client_addr.sin_port = htons (DHCP_PORT_CLIENT);
  client_addr.sin_family = AF_INET;

  if (bind (g_sockfd, (struct sockaddr*) &sockaddr, sizeof (sockaddr)) < 0) {
    log_errno ("Failed to bind socket");
    exit (EXIT_FAILURE);
  }

  for (;;) {
    int ready = poll (&pollfd, 1, poll_timeout);

    if (ready < 0) {
      log_errno ("poll()");
      exit (EXIT_FAILURE);
    }

    /* Check for expired leases */
    time_t now = time (NULL);
    while (g_leaseq.nleases > 0 && now > g_leaseq.leases[0].expire) {
      log_info ("Lease for %s expired", ether_ntoa (&g_leaseq.leases[0].ether_addr));
      as_free (&g_aspace, g_leaseq.leases[0].in_addr);
      lq_pop (&g_leaseq);
    }

    if (ready == 0)
      continue;

    /* Read message */
    struct dhcp_msg msg;
    memset (&msg, 0, sizeof (msg));
    if (recvfrom (g_sockfd, &msg, sizeof (msg), 0, NULL, NULL) < 0) {
      log_errno ("recvfrom()");
      continue;
    }

    if (msg.hlen != ETHER_ADDR_LEN) {
      log_error ("Hardware address length is %d != %d", msg.hlen, ETHER_ADDR_LEN);
      continue;
    }

    log_info ("Received message from %s", ether_ntoa ((struct ether_addr *) msg.chaddr));

    struct dhcp_oit it = dhcp_oit_init(&msg);
    struct dhcp_opt opt;

    if (dhcp_eat_magic_cookie (&it) < 0) {
      log_error ("Failed to parse message: missing or invalid magic cookie");
      continue;
    }

    int have_msg_type = 0;
    enum dhcp_msg_type type;
    while (!it.done) {
      if (dhcp_opt_take (&opt, &it) < 0) {
        log_error ("Error while parsing options");
        break;
      }
      if (opt.tag == DHCP_OPT_DHCP_MESSAGE_TYPE) {
        have_msg_type = 1;
        type = opt.buf[0];
        break;
      }
    }

    if (!have_msg_type)
      continue;

    log_info ("Message type is %s", dhcp_msg_type_str (type));

    switch (type) {
    case DHCP_MSG_TYPE_DHCPDISCOVER:
      process_discover(&msg);
      break;
    // case DHCP_MSG_TYPE_DHCPREQUEST:
    //   process_request(&msg);
    //   break;
    // case DHCP_MSG_TYPE_DHCPRELEASE:
    //   process_release(&msg);
    //   break;
    default:
      log_info ("Unhandled message type");
      break;
    }
  }
}

static void
process_discover (struct dhcp_msg *msg)
{
  time_t now = time (NULL);

  /* Ignore if lease exists */
  for (int i = 0; i < g_leaseq.nleases; i++) {
    struct ether_addr *ether = &g_leaseq.leases[i].ether_addr;
    if (memcmp (ether, msg->chaddr, sizeof (*ether)) == 0) {
      log_info ("Host already has a lease, ignoring discovery message");
      return;
    }
  }

  /* Find static configuration if it exists */
  struct static_conf *sconf = NULL;
  for (int i = 0; i < g_conf.nstatic_confs; i++) {
    struct ether_addr *ether = &g_conf.static_confs[i].ether_addr;
    if (memcmp (ether, msg->chaddr, sizeof (*ether)) == 0) {
      sconf = &g_conf.static_confs[i];
      break;
    }
  }

  /* Determine address and lease time */
  in_addr_t in_addr;
  uint32_t lease_time;
  if (sconf) {
    in_addr = sconf->in_addr;
    lease_time = (uint32_t) sconf->lease_time;
    log_info ("Host is statically configured with address %s",
              inet_str (in_addr));
  } else {
    if (as_alloc (&g_aspace, &in_addr) < 0) {
      log_error ("Out of addresses");
      return;
    }
    lease_time = (uint32_t) g_conf.lease_time;
    log_info ("Host is dynamically assigned address %s",
              inet_str (in_addr));
  }

  /* Reserve address during request window */
  struct lease lease;
  memcpy (&lease.ether_addr, msg->chaddr, sizeof (struct ether_addr));
  lease.in_addr = in_addr;
  lease.expire = now + g_conf.request_window;
  lq_add (&g_leaseq, &lease);

  /* Create reply */
  struct dhcp_msg reply;
  memcpy (&reply, msg, sizeof (reply));

  /* Set message fields */
  reply.op = DHCP_OP_BOOTREPLY;
  reply.hops = 0;
  reply.secs = 0;
  reply.ciaddr = 0;
  reply.yiaddr = in_addr;
  reply.siaddr = 0;

  memset (reply.sname, 0, sizeof (reply.sname));
  memset (reply.file, 0, sizeof (reply.file));
  memcpy (reply.sname, g_hostname, sizeof (reply.sname) - 1);

  /* Set options */
  struct dhcp_opt opt;
  struct dhcp_oit it = dhcp_oit_init (&reply);
  dhcp_add_magic_cookie (&it);

  /* Message type */
  opt.tag = DHCP_OPT_DHCP_MESSAGE_TYPE;
  opt.buf[0] = DHCP_MSG_TYPE_DHCPOFFER;
  opt.len = 1;
  dhcp_opt_add (&opt, &it);

  /* Lease time */
  lease_time = htonl (lease_time);
  opt.tag = DHCP_OPT_IP_ADDRESS_LEASE_TIME;
  memcpy (opt.buf, &lease_time, 4);
  opt.len = 4;
  dhcp_opt_add (&opt, &it);

  /* Server identifier */
  opt.tag = DHCP_OPT_SERVER_IDENTIFIER;
  memcpy (opt.buf, &g_server_addr, 4);
  opt.len = 4;
  dhcp_opt_add (&opt, &it);

  /* Subnet mask */
  opt.tag = DHCP_OPT_SUBNET_MASK;
  memcpy (opt.buf, &g_conf.subnet_mask, 4);
  opt.len = 4;
  dhcp_opt_add (&opt, &it);

  /* End options */
  opt.tag = DHCP_OPT_END_OPTION;
  dhcp_opt_add (&opt, &it);

  /* Send reply */
  log_info ("Replying with %s", dhcp_msg_type_str (DHCP_MSG_TYPE_DHCPOFFER));
  if (sendto (g_sockfd, &reply, sizeof (reply), 0,
              (struct sockaddr*) &client_addr, sizeof (client_addr)) < 0)
    log_errno ("sendto() failed");
}

/* Find address on configured interface */
static int
get_servaddr (void)
{
  struct ifaddrs *ifap, *it;
  if (getifaddrs (&ifap) < 0) {
    log_errno ("getifaddrs()");
    return -1;
  }

  for (it = ifap; it; it = it->ifa_next) {
    if (strcmp (it->ifa_name, g_conf.interface) != 0)
      continue;

    if (it->ifa_addr->sa_family != AF_INET)
      continue;

    g_server_addr = ((struct sockaddr_in*) it->ifa_addr)->sin_addr.s_addr;
    freeifaddrs (ifap);
    return 0;
  }

  log_error ("No address found on device %s", g_conf.interface);
  freeifaddrs (ifap);
  return -1;
}

/* Convert an in_addr_t in to a string */
static char *
inet_str (in_addr_t in_addr)
{
  static char buf[INET_ADDRSTRLEN];
  struct in_addr addr = { .s_addr = in_addr };
  inet_ntop (AF_INET, &addr, buf, sizeof (buf));
  return buf;
}
