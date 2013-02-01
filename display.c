#include <xc.h>
#include <string.h>

#include "display.h"
#include "ledcol.h"
#include "ledrow.h"

unsigned int display_enabled;

display_data fifo_data[DISPLAY_FIFO_LEN];
unsigned int fifo_head;
unsigned int fifo_count;

route routes[DISPLAY_MAX_ROUTES];

route *holds[DISPLAY_ROWS][DISPLAY_COLS];

void display_init(void) {
    fifo_head = 0;
    fifo_count = 0;

    memset(holds, 0, sizeof(holds));
    memset(routes, 0, sizeof(routes));

    display_enabled = 0;
}

void display_enable(void) {
    display_enabled = 1;

    display_process();
}

void display_disable(void) {
    display_enabled = 0;
}

void display_process(void) {

}

void display_setholds(route *sroute, route *holdt) {
    int i;

    for (i = 0; i < sroute->len; i++)
        holds[sroute->holds[i] >> 4][sroute->holds[i] & 0xF] = holdt;
}

void display_showroute(route *theroute) {
    int i = 0;

    for (i = 0; i < DISPLAY_MAX_ROUTES; i++)
        if (routes[i].len == 0) {
            routes[i] = *theroute;
            display_setholds(&routes[i], &routes[i]);
        }
}

void display_hideroute(unsigned int id) {
    int i = 0;

    for (i = 0; i < DISPLAY_MAX_ROUTES; i++)
        if (routes[i].id == id) {
            display_setholds(&routes[i], (0));
            routes[i].id = 0;
            routes[i].len = 0;
        }
}

void fifo_get(display_data *data) {
    __builtin_disi(0x3FFF);
    *data = fifo_data[fifo_head++];
    fifo_head %= DISPLAY_FIFO_LEN;
    fifo_count--;
    __builtin_disi(0);
}

void fifo_put(display_data *data) {
    __builtin_disi(0x3FFF);
    fifo_data[(fifo_head + fifo_count) % DISPLAY_FIFO_LEN] = *data;
    fifo_count++;
    __builtin_disi(0);
}

int fifo_full(void) {
    return (fifo_count == DISPLAY_FIFO_LEN);
}

int fifo_empty(void) {
    return (fifo_count == 0);
}