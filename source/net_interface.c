
 #define _GNU_SOURCE 1 // for strcasestr


#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "net_interface.h"
#include "utils.h"
#include "config.h"

#define RESOLV_CONF_FILE_NAME "/etc/resolv.conf"

const char *get_ip_address_of_nameserver()
{
  FILE *fp;
  char line[512];
  bool found = false;

  // 27 chars for ipv4 and 56 for ipv6
  // udp://x.x.x.x:53
  // udp://[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:53
  static char nameserver[60];

  if ((fp = fopen(RESOLV_CONF_FILE_NAME, "r")) == NULL)
  {
    found = false;
  }
  else
  {
    /* Try to figure out what nameserver to use */
    for (found = false; fgets(line, sizeof(line), fp) != NULL;)
    {
      unsigned int a, b, c, d;
      if (sscanf(line, "nameserver %u.%u.%u.%u", &a, &b, &c, &d) == 4)
      {
        snprintf(nameserver, 60, "udp://%u.%u.%u.%u:53", a, b, c, d);
        found = true;
        break;
      }
    }
    (void)fclose(fp);
  }

  if (found)
    return nameserver;
  else
    return NULL;
}

void url_ptrs(char *url, char **protocol, char **port)
{
  *protocol = NULL;
  *port = NULL;
  char *ptr = (char *)url;

  for (int i = 0; i < strlen(ptr); i++)
  {
    if (ptr[i] == ':' && *protocol == NULL)
    {
      *protocol = &ptr[i];
    }
    else if (ptr[i] == ':' && *protocol != NULL)
    {
      *port = &ptr[i+1];
    }
  }
}

const net_iface *get_first_valid_interface()
{
  static net_iface info;
  static bool found = false;
  struct ifaddrs *ifaddr, *ifa;
  int family;
  
  if (found)
    goto end;

  if (getifaddrs(&ifaddr) == -1)
  {
    perror("getifaddrs");
    return NULL;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
  {
    if (!ifa->ifa_addr)
      continue;

    family = ifa->ifa_addr->sa_family;

    if (!found && family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK))
    {
      struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
      inet_ntop(AF_INET, &sa->sin_addr, info.ip, sizeof(info.ip));
      strncpy(info.name, ifa->ifa_name, sizeof(info.name) - 1);
      info.name[sizeof(info.name) - 1] = '\0';
      found = true;
    }

    if (family == AF_PACKET)
    {
      struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;
      if (s->sll_halen == 6 &&
          (!found || strcmp(info.name, ifa->ifa_name) == 0))
      {
        snprintf(info.mac, sizeof(info.mac),
                 "%02x:%02x:%02x:%02x:%02x:%02x",
                 (unsigned char)s->sll_addr[0],
                 (unsigned char)s->sll_addr[1],
                 (unsigned char)s->sll_addr[2],
                 (unsigned char)s->sll_addr[3],
                 (unsigned char)s->sll_addr[4],
                 (unsigned char)s->sll_addr[5]);

        snprintf(info.rawmac, sizeof(info.rawmac),
                 "%02x%02x%02x%02x%02x%02x",
                 (unsigned char)s->sll_addr[0],
                 (unsigned char)s->sll_addr[1],
                 (unsigned char)s->sll_addr[2],
                 (unsigned char)s->sll_addr[3],
                 (unsigned char)s->sll_addr[4],
                 (unsigned char)s->sll_addr[5]);
      }
    }
  }

  freeifaddrs(ifaddr);

  if (found){
    char *protocol, *port;
    url_ptrs(_aqconfig_.listen_address, &protocol, &port);

    if (port != NULL && protocol != NULL) {
      sprintf(info.url, "%.*s://%s:%s", (int)(protocol - _aqconfig_.listen_address), _aqconfig_.listen_address, info.ip, port);
      sprintf(info.localurl, "%.*s://localhost:%s", (int)(protocol - _aqconfig_.listen_address), _aqconfig_.listen_address, port);
    } else {
      LOG(NET_LOG, LOG_ERR,"Could not understand URL '%s' (unable to get protocol & port) \n",_aqconfig_.listen_address);
    }

    if (_aqconfig_.listen_address[(int)(protocol - _aqconfig_.listen_address)-1] == 's' ||
        _aqconfig_.listen_address[(int)(protocol - _aqconfig_.listen_address)-1] == 'S')
      info.isLocalurlTLS = true;
    else
      info.isLocalurlTLS = false;

    LOG(NET_LOG, LOG_DEBUG, "Interface %s = %s, %s\n", info.name, info.ip, info.mac);
  } else {
    LOG(NET_LOG, LOG_ERR,"Could not find a valid network interface");
  }
  
end:

  return found ? &info : NULL;
}


/* In future maybe remove mqtt_id from aq_config and use static here */
char *generate_mqtt_id() {
  static char ID[MQTT_ID_LEN+1];
  static bool created=false;

  if (!created){
    extern char *__progname; // glibc populates this
    int i;
    strncpy(ID, basename(__progname), MQTT_ID_LEN);
    i = strlen(ID);

    if ( i > 9) { i=9; } // cut down to 9 characters (aqualinkd)
    if (i < MQTT_ID_LEN) {
      ID[i++] = '_';
      const net_iface *info = get_first_valid_interface();
      if (info->rawmac[0] != '\0')
        sprintf(&ID[i], "%.*s", (MQTT_ID_LEN-i), info->rawmac);
      else
        sprintf(&ID[i], "%.*d", (MQTT_ID_LEN-i), getpid());
    }

    ID[MQTT_ID_LEN] = '\0';
    created = true;
  }

  return ID;
}