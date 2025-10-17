#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include "dhcp.h"
#include "dhcp_server.h"
#include "log.h"

struct conf g_conf;
struct lease_queue g_leaseq;
struct addr_space g_aspace;

static const int poll_timeout = 1;

int
main (int argc, char **argv)
{
  const char *conf_path = "./dhcp-server.conf";
  struct pollfd pollfd;
  struct sockaddr_in sockaddr;
  int en_broadcast = 1;
  int sockfd;

  if (argc > 1) {
    conf_path = argv[1];
  }

  if (conf_parse (conf_path, &g_conf) < 0) {
    exit (EXIT_FAILURE);
  }

  as_init (&g_aspace, g_conf.range_lo, g_conf.range_hi);
  lq_init (&g_leaseq);

  if ((sockfd = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
    log_errno ("Failed to open socket");
    exit (EXIT_FAILURE);
  }

  pollfd.fd = sockfd;
  pollfd.events = POLLIN;

  if (setsockopt (sockfd, SOL_SOCKET, SO_BINDTODEVICE,
                  g_conf.interface, strlen (g_conf.interface)) < 0) {
    log_errno ("Failed to bind socket to device %s", g_conf.interface);
    exit (EXIT_FAILURE);
  }

  if (setsockopt (sockfd, SOL_SOCKET, SO_BROADCAST,
                  &en_broadcast, sizeof (en_broadcast)) < 0) {
    log_errno ("Failed to enable broadcasting");
    exit (EXIT_FAILURE);
  }

  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons (DHCP_PORT_SERVER);
  sockaddr.sin_addr.s_addr = INADDR_ANY;

  if (bind (sockfd, (struct sockaddr*) &sockaddr, sizeof (sockaddr)) < 0) {
    log_errno ("Failed to bind socket");
    exit (EXIT_FAILURE);
  }

  for (;;) {
    int ready = poll (&pollfd, 1, poll_timeout);

    if (ready < 0) {
      log_errno ("poll()");
      exit (EXIT_FAILURE);
    }

    time_t now = time (NULL);
    while (g_leaseq.nleases > 0 && now > g_leaseq.leases[0].expire) {
      log_info ("Lease for %s expired", ether_ntoa (&g_leaseq.leases[0].ether_addr));
      as_free (&g_aspace, g_leaseq.leases[0].in_addr);
      lq_pop (&g_leaseq);
    }

    if (ready == 0)
      continue;

    struct dhcp_msg msg;
    memset (&msg, 0, sizeof (msg));
    if (read (sockfd, &msg, sizeof (msg)) < 0) {
      log_errno ("read()");
      continue;
    }

    msg.secs = ntohs (msg.secs);
    msg.flags = ntohs (msg.flags);
    msg.xid = ntohl (msg.xid);

    struct dhcp_opt_iterator it = dhcp_opt_iterator_init(&msg);
    struct dhcp_opt opt;

    if (dhcp_opt_eat_magic_cookie (&it) < 0) {
      log_error ("Failed to parse message: missing or invalid magic cookie");
      continue;
    }

    int have_msg_type = 0;
    enum dhcp_msg_type type;
    while (!it.done) {
      if (dhcp_opt_take_option (&opt, &it) < 0) {
        log_error ("Error while parsing options");
        break;
      }

      if (opt.tag == DHCP_OPT_DHCP_MESSAGE_TYPE) {
        type = opt.buf[0];
        break;
      }
    }

    if (!have_msg_type)
      continue;

    log_info ("Message type is %d", type);
  }
}
