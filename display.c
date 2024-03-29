#include <xc.h>
#include <string.h>

#include "display.h"
#include "ledcol.h"
#include "ledrow.h"

static void display_frequpdate(void);
static void display_clearholds(route *);
static void display_row_recalc(row *);
static void display_setholds(route *);

static void conflict_add(unsigned char, route *);
static void conflict_process(void);
static void conflict_remove(route *);

static void fifo_get(display_data *);
static void fifo_put(display_data *);
static int fifo_full(void);
static int fifo_empty(void);
static void fifo_clear(void);

static unsigned int display_enabled;    /**< Display enabled flag. */
static unsigned int blank;              /**< Whether or not there is at least one hold to display. */

static display_data fifo_data[DISPLAY_FIFO_LEN];    /**< FIFO Data buffer. */
static unsigned int fifo_head;      /**< FIFO head pointer. */
static unsigned int fifo_count;     /**< FIFO entry count. */
static unsigned int fifo_misses;    /**< FIFO underrun count. */

static route routes[DISPLAY_MAX_ROUTES];    /**< Routes active on the display. */
static row rows[DISPLAY_ROWS];              /**< Row data for the display. */

static unsigned int process_activerow;  /**< Active row in output data generator. */
static unsigned int process_pwmpos;     /**< PWM pulse position. */

static unsigned long timer_period;      /**< Display tick period. */
static unsigned int timer_repeat;       /**< Pattern repeat counter. */
static unsigned int timer_activerow;    /**< Active output row. */

static unsigned int hb_pos;             /**< Heartbeat intensity level. */
static unsigned int hb_fullcount;       /**< Heartbeat counter, used for intensity calculation. */

static unsigned int cf_count;           /**< Conflict counter. */
static conflict conflicts[DISPLAY_MAX_CONFLICTS];

/** Initialize display module.
 */
void display_init(void) {
    fifo_head = 0;
    fifo_count = 0;
    fifo_misses = 0;

    memset(rows, 0, sizeof(row)*DISPLAY_ROWS);
    memset(routes, 0, sizeof(route)*DISPLAY_MAX_ROUTES);

    display_enabled = 0;
    blank = 1;

    process_activerow = 0;
    process_pwmpos = 0;

    timer_period = 0;
    timer_repeat = 0;
    timer_activerow = 0;

    hb_pos = 0;
    hb_fullcount = 0;

    cf_count = 0;
    memset(conflicts, 0, sizeof(conflict)*DISPLAY_MAX_CONFLICTS);

    T2CONbits.T32 = 0;
    T2CONbits.TCKPS = 0;

    timer_period = CLOCK_FREQUENCY / DISPLAY_SCAN_FREQ / DISPLAY_COLOR_DEPTH / DISPLAY_ROWS;
//    PR3 = timer_period >> 16;
    PR2 = (unsigned int)(timer_period & 0xFFFF);
    
}

/** Enable display output.
 * Start output display timer.  Fill display package FIFO.
 */
void display_enable(void) {
    display_enabled = 1;
    timer_repeat = 0;
    timer_activerow = DISPLAY_ROWS;

    TMR2 = 0;
//    TMR3 = 0;
    _T2IE = 1;
    T2CONbits.TON = 1;
    _T2IP = DISPLAY_INT_PRIORITY;

    if (fifo_empty())
        display_process();
    else
        _T2IF = 1;
}

/** Disable display output.
 */
void display_disable(void) {
    display_enabled = 0;

    fifo_clear();
}

/** Timer3 Interrupt Service Routine.
 * Update the display frame.
 */
void __attribute__((interrupt, auto_psv)) _T2Interrupt(void) {
    _T2IF = 0;

    if (!display_enabled) { // if disabled, clear the display, stop timer
        ledrow_disable();   // cut row driver
        ledcol_clear();     // cut column drive
        T2CONbits.TON = 0;
        _T2IE = 0;
        return;
    }

    if (blank)
        return;

    if (timer_repeat--)     // display same column data
        return;

    if (!fifo_empty()) {    // display next frame
        display_data dd;

        fifo_get(&dd);      // remove packet from fifo
        ledcol_display(&dd.cdata);  // send packet to column drivers
        if (timer_activerow != dd.row) {    // if new row, switch
            ledrow_switch(dd.row);
            timer_activerow = dd.row;
        }
        timer_repeat = dd.repeat;
    } else {
        fifo_misses++;
        timer_repeat++;
    }
}

/** Recalculate optimal display update frequency.
 * Analyzes the total number of frames per cycle, and adjusts the timer to
 * maintain the scan frequency.  It is called automatically by showroute and
 * hideroute.
 */
static void display_frequpdate(void) {
    unsigned int pulse_count = 0;
    int i;

    for (i = 0; i < DISPLAY_ROWS; i++)          // count frames per cycle
        pulse_count += rows[i].maxbrightness;
    if (!pulse_count)
        pulse_count = 32;

    timer_period = CLOCK_FREQUENCY / DISPLAY_SCAN_FREQ / pulse_count;
//    PR3 = timer_period >> 16;
    PR2 = (unsigned int)(timer_period & 0xFFFF);
    TMR2 = 0;
//    TMR3 = 0;
}

inline void display_hbupdate(void) {
    hb_fullcount = (hb_fullcount + 1) % (DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ);
    if (hb_fullcount >= DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ / 2)
        hb_pos = (DISPLAY_COLOR_DEPTH * (DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ - hb_fullcount - 1) * 2) / (DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ) + 1;
    else
        hb_pos = (DISPLAY_COLOR_DEPTH * hb_fullcount * 2) / (DISPLAY_HEARTBEAT_PERIOD * DISPLAY_SCAN_FREQ) + 1;
}

/** Process the display module.
 * This generates frames to send to the LED drivers.  It will generate frames
 * until the FIFO is full.  This module handles software PWM, heartbeat mode,
 * and row skip optimizations.  It should be called periodically while the
 * display is active.
 */
void display_process(void) {
    static display_data cbuffer;
    static unsigned char pwmflag[DISPLAY_COLOR_DEPTH];
    row *prow;
    route *phold;
    int i;
    int pcount = 0;

    if (blank || !display_enabled)
        return;

    while (!fifo_full() && pcount < 3) {      // generate frames until the buffer is full
        if (!rows[process_activerow].enabled) {     // if row isn't active, skip
            process_activerow++;
            process_activerow %= DISPLAY_ROWS;
            if (process_activerow == 0) {   // scanned through entire array
                display_hbupdate();
                cf_count = (cf_count + 1) % (DISPLAY_SCAN_FREQ * DISPLAY_FLASH_PERIOD);
                if (!cf_count)
                    conflict_process();
            }
            continue;
        }
        
        prow = &rows[process_activerow];    // pointer to active row for convenience

        if (process_pwmpos == 0) {      // first entry for this row
            memset(&cbuffer, 0, sizeof(display_data));
            memset(pwmflag, 0, DISPLAY_COLOR_DEPTH);
            
            cbuffer.row = process_activerow;        
            for (i = 0; i < DISPLAY_COLS; i++)      // loop through cols in the row
                if ((phold = prow->holds[i])) {     // enable any hold that needs lighting
                    if (phold->heartbeat) {         // heartbeat holds may have reduced active time
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
                    } else {    // turn on all holds that will be active on this row
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
        } else {    // not the first cycle in a row, turn off based on pwm value
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

        while (!pwmflag[process_pwmpos] &&          // check if pattern repeats
                process_pwmpos < prow->maxbrightness) {
            process_pwmpos++;
            cbuffer.repeat++;
        }

        fifo_put(&cbuffer);
        pcount++;

        if (process_pwmpos >= prow->maxbrightness) {    // end of row
            process_activerow = (process_activerow + 1) % DISPLAY_ROWS;
            process_pwmpos = 0;
            if (process_activerow == 0) {   // scanned through entire array
                display_hbupdate();
                cf_count = (cf_count + 1) % (DISPLAY_SCAN_FREQ * DISPLAY_FLASH_PERIOD);
                if (!cf_count)
                    conflict_process();
            }
        }
    }

}

/** Sets all of the holds in route in the row data structure.
 */
static void display_setholds(route *sroute) {
    int i;

    int mbright = MAX(sroute->r, sroute->g, sroute->b);

    for (i = 0; i < sroute->len; i++) {
        blank = 0;
        int ppos = display_translate(sroute->holds[i]);

        int r = ppos >> 4;
        int c = ppos & 0xF;

        if (rows[r].holds[c])
            conflict_add(ppos, sroute);
        else
            rows[r].holds[c] = sroute;
        
        rows[r].enabled = 1;
        if (rows[r].maxbrightness < mbright)
            rows[r].maxbrightness = mbright;
    }
}

/** Recalculate the row summary fields for use in the display process.
 * This is called internally and automatically.
 */
static void display_row_recalc(row *r) {
    int mbright = 0;
    int i;
    route *troute;

    r->enabled = 0;
    r->maxbrightness = 0;

    for (i = 0; i < DISPLAY_COLS; i++)      // update enabled and maxbrightness
        if ((troute = r->holds[i])) {
            r->enabled = 1;
            mbright = MAX(troute->r, troute->g, troute->b);
            if (mbright > r->maxbrightness)
                r->maxbrightness = mbright;
        }
}

/** Remove all holds in the route from the row table.
 */
static void display_clearholds(route *sroute) {
    int i;
    route *tmp;

    for (i = 0; i < sroute->len; i++) {
        int ppos = display_translate(sroute->holds[i]);
        int r = ppos >> 4;
        int c = ppos & 0xF;
        if (rows[r].holds[c] == sroute)
            rows[r].holds[c] = NULL;
        display_row_recalc(&rows[r]);
    }
}

/** Show the route.
 */
void display_showroute(route *theroute) {
    int i;

    for (i = 0; i < DISPLAY_MAX_ROUTES; i++)
        if (routes[i].id == theroute->id) {
            display_clearholds(&routes[i]);   // route already displayed, remove and then re-add
            conflict_remove(&routes[i]);
            break;
        }

    if (i >= DISPLAY_MAX_ROUTES) 
        for (i = 0; i < DISPLAY_MAX_ROUTES; i++)
            if (routes[i].len == 0)
                break;

    if (i < DISPLAY_MAX_ROUTES) {
        routes[i] = *theroute;
        routes[i].r >>= (8 - DISPLAY_COLOR_DEPTH_BITS);
        routes[i].g >>= (8 - DISPLAY_COLOR_DEPTH_BITS);
        routes[i].b >>= (8 - DISPLAY_COLOR_DEPTH_BITS);
        display_setholds(&routes[i]);
        display_frequpdate();
        fifo_clear();
    }
}

/** Hide the route. */
void display_hideroute(unsigned int id) {
    int i = 0;
    int nblank = 1;

    for (i = 0; i < DISPLAY_MAX_ROUTES; i++)
        if (routes[i].id == id) {
            display_clearholds(&routes[i]);
            conflict_remove(&routes[i]);
            routes[i].id = 0;
            routes[i].len = 0;
        }

    for (i = 0; i < DISPLAY_ROWS; i++)
        if (rows[i].enabled)
            nblank = 0;

    if (nblank) {
        blank = 1;
        _T2IE = 0;          // TODO: is there a cleaner way to do this, is this safe?
        ledcol_clear();
        _T2IE = 1;
    }

    fifo_clear();
    display_frequpdate();
}

/** Clear all routes from the display.
 */
void display_clearroutes(void) {
    int i, j;

    blank = 1;

    _T2IE = 0;          // TODO: is there a cleaner way to do this, is this safe?
    ledcol_clear();
    _T2IE = 1;

    for (i = 0; i < DISPLAY_MAX_ROUTES; i++) {
        routes[i].id = 0;
        routes[i].len = 0;
    }

    for (i = 0; i < DISPLAY_ROWS; i++)
        for (j = 0; j < DISPLAY_COLS; j++)
            rows[i].holds[j] = NULL;

    for (i = 0; i < DISPLAY_MAX_CONFLICTS; i++)
        conflicts[i].route = NULL;

    fifo_clear();
    display_disable();
}

/** Process conflict list, replacing active holds on display with those in the list.
 */
static void conflict_process(void) {
    route *tmp;
    conflict *cf;
    int i;

    for (i = 0; i < DISPLAY_MAX_CONFLICTS; i++) {
        cf = &conflicts[i];
        if (cf->route && cf->head) {    // we have a conflict
            tmp = rows[cf->row].holds[cf->col];
            rows[cf->row].holds[cf->col] = cf->route;   // replace displayed hold with list head
            while (cf->next) {
                if (cf->next->route) {                          // bubble forward every hold in the list
                    cf->route = cf->next->route;
                    cf = cf->next;
                } else {
                    cf->next = NULL;
                }
            }
            cf->route = tmp;                            // place old displayed hold at end of the list

            // TODO: recalc row?
        }
    }
}

/** Add an entry to the conflict list.
 */
static void conflict_add(unsigned char pos, route *theroute) {
    conflict *gt2tail = NULL;
    conflict *cf;
    int i;
    
    for (i = 0; i < DISPLAY_MAX_CONFLICTS; i++)
        if (conflicts[i].pos == pos && conflicts[i].route && !conflicts[i].next)
            gt2tail = &conflicts[i];

    for (i = 0; i < DISPLAY_MAX_CONFLICTS; i++)
        if (!conflicts[i].route) {
            cf = &conflicts[i];
            if (gt2tail) {
                gt2tail->next = cf;
                cf->head = 0;
            } else {
                cf->head = 1;
            }
            cf->pos = pos;
            cf->route = theroute;
            cf->next = NULL;
            break;
        }
}

/** Remove all conflict instances for the route passed.
 */
static void conflict_remove(route *theroute) {
    conflict *cf;
    int i;

    for (i = 0; i < DISPLAY_MAX_CONFLICTS; i++)
        if (conflicts[i].route == theroute) {       // we have a match
            cf = &conflicts[i];
            if (cf->next) {     // if it has a next, move it to here, and null the next
                cf->route = cf->next->route;
                cf->next->route = NULL;
                cf->next = cf->next->next;  // could be NULL, don't care
            } else {    // no next, remove this conflict
                cf->route = NULL;
            }
        }

    for (i = 0; i < DISPLAY_MAX_CONFLICTS; i++) // clean up next fields that point to NULL conflict
        if (conflicts[i].next)
            if (conflicts[i].next->route == NULL)
                conflicts[i].next = NULL;

}

/** Translate logical position to physical position.
 */
inline unsigned char display_translate(unsigned char lpos) {
    unsigned char ppos = lpos & 0xF;
    unsigned char p;

    if (lpos % 2) { // odd column
        if ((p = (lpos & 0xF0)) < 64)  // bottom half
            ppos += p << 1;
        else
            ppos += ((p - 64) << 1) + 16;
    } else {
        if ((p = (lpos & 0xF0)) < 64)
            ppos += (p << 1) + 16;
        else
            ppos += ((p - 64) << 1);
    }

    return ppos;
}

/** Get entry from the FIFO.
 */
static void fifo_get(display_data *data) {
    __builtin_disi(0x3FFF);
    *data = fifo_data[fifo_head++];
    fifo_head %= DISPLAY_FIFO_LEN;
    fifo_count--;
    __builtin_disi(0);
}

/** Put entry into the FIFO.
 */
static void fifo_put(display_data *data) {
    __builtin_disi(0x3FFF);
    fifo_data[(fifo_head + fifo_count) % DISPLAY_FIFO_LEN] = *data;
    fifo_count++;
    __builtin_disi(0);
}

/** Clear the FIFO. */
static void fifo_clear(void) {
    fifo_count = 0;         // atomic write
}

/** Check if FIFO is full. */
static inline int fifo_full(void) {
    return (fifo_count == DISPLAY_FIFO_LEN);
}

/** Check if FIFO is empty. */
static inline int fifo_empty(void) {
    return (fifo_count == 0);
}