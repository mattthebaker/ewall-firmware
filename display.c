#include <xc.h>
#include <string.h>

#include "display.h"
#include "ledcol.h"
#include "ledrow.h"

unsigned int display_enabled;

display_data fifo_data[DISPLAY_FIFO_LEN];
unsigned int fifo_head;
unsigned int fifo_count;
unsigned int fifo_misses;

route routes[DISPLAY_MAX_ROUTES];
row rows[DISPLAY_ROWS];

unsigned int process_activerow;
unsigned int process_pwmpos;

unsigned long timer_period;
unsigned int timer_repeat;
unsigned int timer_activerow;

unsigned int hb_pos;
unsigned int hb_fullcount;

void display_init(void) {
    fifo_head = 0;
    fifo_count = 0;
    fifo_misses = 0;

    memset(rows, 0, sizeof(rows));
    memset(routes, 0, sizeof(routes));

    display_enabled = 0;

    process_activerow = 0;
    process_pwmpos = 0;

    timer_period = 0;
    timer_repeat = 0;
    timer_activerow = 0;

    hb_pos = 0;
    hb_fullcount = 0;

    T2CONbits.T32 = 1;
    T2CONbits.TCKPS = 0;
    PR3 = 0;
    PR2 = 0;
    
}

void display_enable(void) {
    display_enabled = 1;
    timer_repeat = 0;
    timer_activerow = DISPLAY_ROWS;

    TMR2 = 0;
    TMR3 = 0;
    _T3IE = 1;
    T2CONbits.TON = 1;

    if (fifo_empty())
        display_process();
    else
        _T3IF = 1;
}

void display_disable(void) {
    display_enabled = 0;

    fifo_clear();
}

void __attribute__((interrupt, shadow, auto_psv)) _T3Interrupt(void) {
    _T3IF = 0;

    if (!display_enabled) {
        ledrow_disable();
        ledcol_clear();
        T2CONbits.TON = 0;
        _T3IE = 0;
        return;
    }

    if (timer_repeat--)     // display same column data
        return;

    if (!fifo_empty()) {
        display_data dd;

        fifo_get(&dd);
        ledcol_display(&dd.cdata);
        if (timer_activerow != dd.row) {
            ledrow_switch(dd.row);
            timer_activerow = dd.row;
        }
        timer_repeat = dd.repeat;
    } else {
        fifo_misses++;
    }
}

void display_frequpdate(void) {
    unsigned int pulse_count = 0;
    int i;

    for (i = 0; i < DISPLAY_ROWS; i++)
        pulse_count += rows[i].maxbrightness;
    if (!pulse_count)
        pulse_count = 32;

    timer_period = CLOCK_FREQUENCY / DISPLAY_SCAN_FREQ / pulse_count;
    PR3 = timer_period >> 16;
    PR2 = (unsigned int)(timer_period & 0xFFFF);
    TMR2 = 0;
    TMR3 = 0;
}

void display_process(void) {
    static display_data cbuffer;
    static unsigned char pwmflag[DISPLAY_COLOR_DEPTH];
    row *prow;
    route *phold;
    int i;

    while (!fifo_full()) {
        if (!rows[process_activerow].enabled) {     // if row isn't active, skip
            process_activerow++;
            process_activerow %= DISPLAY_ROWS;
            continue;
        }
        
        prow = &rows[process_activerow];

        if (process_pwmpos == 0) {      // first entry for this row
            memset(&cbuffer, 0, sizeof(display_data));
            memset(&pwmflag, 0, sizeof(pwmflag));
            
            cbuffer.row = process_activerow;        
            for (i = 0; i < DISPLAY_COLS; i++)      // TODO: consider optimizing
                if ((phold = prow->holds[i])) {     // enable any hold that needs lighting
                    if (phold->heartbeat) {
                        unsigned int phigh;         // high pulse length
                        phigh = phold->r * hb_pos / DISPLAY_COLOR_DEPTH;
                        if (phigh) {
                            ledcol_bitset_r(&cbuffer.cdata, i);
                            pwmflag[phigh] = 1;
                        }
                        phigh = phold->g * hb_pos / DISPLAY_COLOR_DEPTH;
                        if (phigh) {
                            ledcol_bitset_g(&cbuffer.cdata, i);
                            pwmflag[phigh] = 1;
                        }
                        phigh = phold->b * hb_pos / DISPLAY_COLOR_DEPTH;
                        if (phigh) {
                            ledcol_bitset_b(&cbuffer.cdata, i);
                            pwmflag[phigh] = 1;
                        }
                    } else {
                        if (phold->r) {
                            ledcol_bitset_r(&cbuffer.cdata, i);
                            pwmflag[phold->r] = 1;
                        }
                        if (phold->g) {
                            ledcol_bitset_g(&cbuffer.cdata, i);
                            pwmflag[phold->g] = 1;
                        }
                        if (phold->b) {
                            ledcol_bitset_b(&cbuffer.cdata, i);
                            pwmflag[phold->b] = 1;
                        }
                    }
                }
        } else {
            cbuffer.repeat = 0;
            for (i = 0; i < DISPLAY_COLS; i++)
                if ((phold = prow->holds[i])) {
                    if (phold->heartbeat) {
                        if (phold->r * hb_pos / DISPLAY_COLOR_DEPTH == process_pwmpos)
                             ledcol_bitclr_r(&cbuffer.cdata, i);
                        if (phold->g * hb_pos / DISPLAY_COLOR_DEPTH == process_pwmpos)
                            ledcol_bitclr_g(&cbuffer.cdata, i);
                        if (phold->b * hb_pos / DISPLAY_COLOR_DEPTH == process_pwmpos)
                            ledcol_bitclr_b(&cbuffer.cdata, i);
                    } else {
                        if (phold->r == process_pwmpos)
                             ledcol_bitclr_r(&cbuffer.cdata, i);
                        if (phold->g == process_pwmpos)
                            ledcol_bitclr_g(&cbuffer.cdata, i);
                        if (phold->b == process_pwmpos)
                            ledcol_bitclr_b(&cbuffer.cdata, i);
                    }

                }            
        }
        process_pwmpos++;   // current position has been processed

        while (!pwmflag[process_pwmpos] && 
                process_pwmpos < prow->maxbrightness) {
            process_pwmpos++;
            cbuffer.repeat++;
        }

        fifo_put(&cbuffer);

        if (process_pwmpos >= DISPLAY_COLOR_DEPTH) {    // end of row
            process_activerow = (process_activerow + 1) % DISPLAY_ROWS;
            process_pwmpos = 0;
            if (process_activerow == 0) {   // scanned through entire array
                hb_fullcount = (hb_fullcount + 1) % (DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ);
                if (hb_fullcount >= DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ / 2)
                    hb_pos = (DISPLAY_COLOR_DEPTH * (DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ - hb_fullcount - 1) * 2) / (DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ) + 1;
                else
                    hb_pos = (DISPLAY_COLOR_DEPTH * hb_fullcount * 2) / (DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ) + 1;
            }
        }
    }
}

void display_setholds(route *sroute) {
    int i;

    int mbright = MAX(sroute->r, sroute->g, sroute->b);

    for (i = 0; i < sroute->len; i++) {
        int r = sroute->holds[i] >> 4;
        int c = sroute->holds[i] & 0xF;
        rows[r].holds[c] = sroute;
        rows[r].enabled = 1;
        if (rows[r].maxbrightness < mbright)
            rows[r].maxbrightness = mbright;
    }
}

void display_row_recalc(row *r) {
    int mbright = 0;
    int i;
    route *troute;

    r->enabled = 0;

    for (i = 0; i < DISPLAY_COLS; i++)
        if ((troute = r->holds[i])) {
            r->enabled = 1;
            mbright = MAX(troute->r, troute->g, troute->b);
        }
    r->maxbrightness = mbright;
}

void display_clearholds(route *sroute) {
    int i;

    for (i = 0; i < sroute->len; i++) {
        int r = sroute->holds[i] >> 4;
        int c = sroute->holds[i] & 0xF;
        rows[r].holds[c] = (0);
        display_row_recalc(&rows[r]);
    }
}

void display_showroute(route *theroute) {
    int i = 0;

    for (i = 0; i < DISPLAY_MAX_ROUTES; i++)
        if (routes[i].len == 0) {
            routes[i] = *theroute;
            display_setholds(&routes[i]);
        }

    display_frequpdate();
}

void display_hideroute(unsigned int id) {
    int i = 0;

    for (i = 0; i < DISPLAY_MAX_ROUTES; i++)
        if (routes[i].id == id) {
            display_clearholds(&routes[i]);
            routes[i].id = 0;
            routes[i].len = 0;
        }

    display_frequpdate();
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

void fifo_clear(void) {
    fifo_count = 0;         // atomic write
}

inline int fifo_full(void) {
    return (fifo_count == DISPLAY_FIFO_LEN);
}

inline int fifo_empty(void) {
    return (fifo_count == 0);
}