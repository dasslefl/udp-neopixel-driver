
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

#define LED_PIO pio1
#define LED_PIO_HW pio1_hw

#define LED_BASE_PIN 6
#define LED_STRING_COUNT 4

#define LED_FREQ 800000

struct udp_pcb *udp_pcb;

typedef struct {
    uint pin;
    uint offset;

    uint sm;

    dma_channel_config dma_config;
    uint32_t dma_chan;
} led_t;

static led_t led_strips[LED_STRING_COUNT];
static uint32_t framebuffer[PIXELS_PER_STRING * LED_STRING_COUNT]; // ein großer Framebuffer

static void udp_packet_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {

    if (p != NULL) {
    
        if(p->tot_len == PIXELS_PER_STRING * LED_STRING_COUNT * 3 && !dma_channel_is_busy(led_strips[0].dma_chan)) { // nicht sauber aber sollte gehen
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
            for(uint i = 0; i < LED_STRING_COUNT; i++) {
                led_t * strip = &(led_strips[i]);

                dma_channel_configure(strip->dma_chan, &(strip->dma_config),
                    &(LED_PIO_HW->txf[strip->sm]),        // Destination pointer
                    framebuffer + strip->offset,      // Source pointer
                    PIXELS_PER_STRING, // Number of transfers
                    true       // Start immediately
                );
            }
        }

        // free the pbuf 
        pbuf_free(p);
    }
}

void udp2pixel_init(uint16_t port) {
    
    // PIO
    uint offset = pio_add_program(LED_PIO, &ws2812_program);

    for(uint i = 0; i < LED_STRING_COUNT; i++) {
        // struct bestücken
        led_t * strip = &led_strips[i];

        strip->pin = LED_BASE_PIN + i;
        strip->offset = i * PIXELS_PER_STRING;
        strip->sm = i;

        strip->dma_chan = dma_claim_unused_channel(true);

        strip->dma_config = dma_channel_get_default_config(strip->dma_chan);
        channel_config_set_read_increment(&(strip->dma_config), true);
        channel_config_set_write_increment(&(strip->dma_config), false);
        channel_config_set_dreq(&(strip->dma_config), pio_get_dreq(LED_PIO, strip->sm, true));

        // SM
        uint sm = strip->sm;

        pio_gpio_init(LED_PIO, strip->pin);
        pio_sm_set_consecutive_pindirs(LED_PIO, sm, strip->pin, 1, true);

        pio_sm_config c = ws2812_program_get_default_config(offset);
        sm_config_set_sideset_pins(&c, strip->pin);
        sm_config_set_out_shift(&c, false, true, 24);
        sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

        int cycles_per_bit = ws2812_T1 + ws2812_T2 + ws2812_T3;
        float div = clock_get_hz(clk_sys) / (LED_FREQ * cycles_per_bit);
        sm_config_set_clkdiv(&c, div);

        pio_sm_init(LED_PIO, sm, offset, &c);
        pio_sm_set_enabled(LED_PIO, sm, true);
    }

    // UDP init
    udp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (udp_pcb != NULL) {
        err_t err;

        err = udp_bind(udp_pcb, IP_ANY_TYPE, port);

        if (err == ERR_OK) {
            udp_recv(udp_pcb, udp_packet_recv, NULL); // Pointer auf Streifen
        } else {
            printf("Error on bind.\n");
            while(1);
        }
    } else {
        printf("Error on init.\n");
        while(1);
    }

}