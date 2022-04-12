
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include "udp2pixel.h"

#include "eth_lwip.h"

#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/udp.h"
#include "lwip/apps/httpd.h"

static struct udp_pcb *udpecho_raw_pcb;

void netif_link_callback(struct netif *netif) {
    printf("netif link status changed %s\n", netif_is_link_up(netif) ? "up" : "down");
}

void netif_status_callback(struct netif *netif) {
    printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

int main() {
    struct netif netif;
    // Also runs stdio init
    eth_lwip_init(&netif);

    sleep_ms(5000);

    printf("UDP Neopixel Driver\n");

    // assign callbacks for link and status
    netif_set_link_callback(&netif, netif_link_callback);
    netif_set_status_callback(&netif, netif_status_callback);

    // set the default interface and bring it up
    netif_set_default(&netif);
    netif_set_up(&netif);

    // Start DHCP client
    dhcp_start(&netif);
    
    httpd_init();

    udp2pixel_init(7000);
    
    while (true) {
        eth_lwip_poll();
    }
}