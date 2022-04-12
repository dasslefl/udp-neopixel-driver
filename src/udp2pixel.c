
#include <stdint.h>
#include <string.h>

#include "lwip/udp.h"

#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "ws2812.pio.h"
#include "udp2pixel.h"

#define PIXELS_PER_STRING 60 

#define LED_PIN 8
#define LED_PIO pio1
#define LED_PIO_HW pio1_hw

#define LED_FREQ 800000

static uint32_t framebuffer[PIXELS_PER_STRING];

static uint led_dma_chan;

static struct udp_pcb *udpecho_raw_pcb;

static void udp_packet_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    LWIP_UNUSED_ARG(arg);
    if (p != NULL) {
    
        if(p->tot_len == PIXELS_PER_STRING * 3 && !dma_channel_is_busy(led_dma_chan)) {
            // Datagram in Framebuffer kopieren
            // Simples Übersetzen RGB -> GRB
            uint color = 0;
            uint led = 0;

            for (struct pbuf *q = p; q != NULL; q = q->next) {
                
                for(uint i = 0; i < q->len; i++) {

                    if(color == 0) framebuffer[led] = ((uint32_t) ((uint8_t *) q->payload)[i]) << 16; // R
                    if(color == 1) framebuffer[led] |= ((uint32_t) ((uint8_t *) q->payload)[i]) << 24; // G
                    if(color == 2) framebuffer[led] |= ((uint32_t) ((uint8_t *) q->payload)[i]) << 8;  // B

                    color++;
                    if(color == 3) {
                        color = 0;
                        led++;
                    }
                }

                if (q->len == q->tot_len) {
                    break;
                }
            }

            // raussenden
            dma_channel_config c = dma_channel_get_default_config(led_dma_chan);
            channel_config_set_read_increment(&c, true);
            channel_config_set_write_increment(&c, false);
            channel_config_set_dreq(&c, pio_get_dreq(LED_PIO, 0, true));

            dma_channel_configure(led_dma_chan, &c,
                &(LED_PIO_HW->txf[0]),        // Destination pointer
                framebuffer,      // Source pointer
                PIXELS_PER_STRING, // Number of transfers
                true       // Start immediately
            );
        }

        // free the pbuf 
        pbuf_free(p);
    }
}

void udp2pixel_init(uint16_t port) {
    // DMA
    led_dma_chan = dma_claim_unused_channel(true);
    
    // PIO
    uint offset = pio_add_program(LED_PIO, &ws2812_program);
    // SM
    uint sm = 0;
    pio_gpio_init(LED_PIO, LED_PIN);
    pio_sm_set_consecutive_pindirs(LED_PIO, sm, LED_PIN, 1, true);

    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, LED_PIN);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    int cycles_per_bit = ws2812_T1 + ws2812_T2 + ws2812_T3;
    float div = clock_get_hz(clk_sys) / (LED_FREQ * cycles_per_bit);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(LED_PIO, sm, offset, &c);
    pio_sm_set_enabled(LED_PIO, sm, true);

    // UDP init
    udpecho_raw_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (udpecho_raw_pcb != NULL) {
        err_t err;

        err = udp_bind(udpecho_raw_pcb, IP_ANY_TYPE, port);

        if (err == ERR_OK) {
            udp_recv(udpecho_raw_pcb, udp_packet_recv, NULL);
        } else {
            /* abort? output diagnostic? */
        }
    } else {
        /* abort? output diagnostic? */
    }

}