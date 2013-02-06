#include <xc.h>

#include "touch.h"

/** Struct that represents a touch channel. */
typedef struct {
    unsigned port:2;
    unsigned pindex:4;
    unsigned amux:2;
    unsigned aindex:4;
} touch_channel;

enum cmat {CMAT_PORT, CMAT_POS, CMAT_AMUX, CMAT_AN};

/** Const array that contains touch channel->peripheral mapping information. */
const touch_channel cmatrix[TOUCH_CHANNEL_COUNT] = {{1, 2, 0, 4},
                                                    {0, 1, 0, 1},
                                                    {1, 15, 0, 9},
                                                    {1, 14, 2, 10},
                                                    {1, 14, 0, 10},
                                                    {1, 14, 3, 10},
                                                    {1, 14, 1, 10},
                                                    {1, 13, 2, 11},
                                                    {1, 13, 1, 11},
                                                    {1, 13, 0, 11},
                                                    {1, 13, 3, 11},
                                                    {2, 3, 0, 12},
                                                    {2, 2, 0, 8},
                                                    {2, 1, 0, 9},
                                                    {1, 3, 2, 5},
                                                    {1, 3, 0, 5},
                                                    {1, 3, 3, 5},
                                                    {1, 3, 1, 5},
                                                    {2, 0, 2, 6},
                                                    {2, 0, 1, 6},
                                                    {2, 0, 0, 6},
                                                    {2, 0, 3, 6}};

volatile unsigned int * const ports[3] = {&PORTA, &PORTB, &PORTC};
volatile unsigned int * const tris[3] = {&TRISA, &TRISB, &TRISC};

unsigned int enabled = 0;
unsigned int active_channel = 0;

unsigned int avg_depth = 0;
unsigned int samples[TOUCH_CHANNEL_COUNT];
unsigned int basecount[TOUCH_CHANNEL_COUNT];
unsigned int threshold_count[TOUCH_CHANNEL_COUNT];

void (*press_cb)(unsigned int);
void (*release_cb)(unsigned int);

unsigned int status_change = 0;
unsigned long prev_touch = 0;
unsigned long cur_touch = 0;

void touch_init(void) {
    int i;

    TMR1 = 0;                   // configure timer
    PR1 = TOUCH_TIME_CONSTANT;
    _T1IP = TOUCH_CPU_PRIORITY;                  //

    // CTMU configuration
    _CTMUEN = 0;                // disable
    _EDGSEQEN = 1;              // require two edges for current pulse
    _EDG2POL = 1;               // rising edge
    _EDG1POL = 1;
    _EDG1SEL = 1;               // TODO: Is this right? Want to manually set this edge.
    _CTTRIG = 1;                // trigger A/D conversion when current shuts off
    _IRNG = 2;                  // 5.5 uA
    _IDISSEN = 1;               // ground current source
    _EDG2STAT = 0;              // clear edge status
    _EDG1STAT = 0;

    // ADC configuration
    _SSRC = 6;                  // CTMU triggers conversion
    _CH0SA = 1;                 // set default input to a grounded touch channel
    _ADON = 0;
    _AD1IP = TOUCH_CPU_PRIORITY;    // TODO: determine proper interrupt priority, should be low prio

    for (i = 0; i < TOUCH_CHANNEL_COUNT; i++) {
        samples[i] = 0;
        basecount[i] = 0;
        threshold_count[i] = 0;
    }
}

void touch_setcallbacks(void (*press)(unsigned int), void (*release)(unsigned int)) {
    press_cb = press;
    release_cb = release;
}

void touch_enable(void) {
    _CTMUEN = 1;
    _EDG2STAT = 0;              // clear edge status
    _EDG1STAT = 0;
    _EDGEN = 1;                 // enabled edge inputs

    _ADON = 1;
    _AD1IF = 0;
    _AD1IE = 1;    

    enabled = 1;

    status_change = 0;
    prev_touch = 0;
    cur_touch = 0;
}

void touch_next(void) {
    unsigned int cpu_priority;
    touch_channel prev_chan;
    touch_channel cur_chan;

    prev_chan = cmatrix[active_channel];

    if (++active_channel >= TOUCH_CHANNEL_COUNT)
        active_channel = 0;

    cur_chan = cmatrix[active_channel];

    RMBITW(*(tris[prev_chan.port]), prev_chan.pindex, 0);   // ground inactive channel
    RMBITW(*(ports[prev_chan.port]), prev_chan.pindex, 0);  // TODO: shouldn't need this (value should always be 0).

    TOUCH_AMUX_AMSEL0 = cur_chan.amux % 2;    // switch amux
    TOUCH_AMUX_AMSEL1 = cur_chan.amux / 2;

    _CH0SA = cur_chan.aindex;   // switch ADC channel
    Nop(); Nop(); Nop();        // wait a little bit for ADC cap to discharge
    RMBITW(*(tris[cur_chan.port]), cur_chan.pindex, 1);     // set pin to input

    TMR1 = 0;       // reset timer count

    _IDISSEN = 0;   // disable current ground override
    _EDG1STAT = 0;  // clear CTMU triggers
    _EDG2STAT = 0;

    SET_AND_SAVE_CPU_IPL(cpu_priority, 7);  // disable interrupts for critical timing
    _EDG1STAT = 1;  // enable current source
    _TON = 1;       // enable timer
    RESTORE_CPU_IPL(cpu_priority);          // re-enable interrupts
}

void touch_interval_delay(void) {
    TMR1 = 0;
    _TCKPS = 1;     // 8:1 prescaler
    PR1 = TOUCH_SAMPLING_DELAY;

    _T1IF = 0;
    _T1IE = 1;
    _T1IP = 1;
    _TON = 1;
}

void touch_process_samples() {
    unsigned int thresh_exceeded = 0;
    unsigned int i;

    prev_touch = cur_touch;
    cur_touch = 0;

    if (avg_depth < TOUCH_AVG_DEPTH) {      // accumulate baseline before detecting touch
        for (i = 0; i < TOUCH_CHANNEL_COUNT; i++)
            basecount[i] += samples[i];
        avg_depth++;
        return;
    }

    for (i = 0; i < TOUCH_CHANNEL_COUNT; i++) {
        if ((samples[i] - (basecount[i] / TOUCH_AVG_DEPTH)) > TOUCH_DETECT_THRESHOLD) {
            thresh_exceeded++;
            threshold_count[i]++;
            if (threshold_count[i] == 3) {
                if (press_cb)
                    press_cb(i);
                cur_touch |= 1 << i;
            }
        }
        else
            if (prev_touch & ~(1 << i)) {
                if (release_cb)
                    release_cb(i);
                threshold_count[i] = 0;
            }
    }

    if (prev_touch != cur_touch)
        status_change = 1;

    if (!thresh_exceeded)   // if no finger is near, apply current sample to average
        for (i = 0; i < TOUCH_CHANNEL_COUNT; i++)
            basecount[i] += samples[i] - (basecount[i] / avg_depth);

}


void __attribute__((interrupt, shadow, auto_psv)) _ADC1Interrupt(void) {
    _AD1IF = 0;

    _TON = 0;   // TODO: this may need to be shut off in the timer interrupt
    _IDISSEN = 1;

    samples[active_channel] = ADC1BUF0;

    if (active_channel == TOUCH_CHANNEL_COUNT - 1) {
        touch_process_samples();

        if (avg_depth == TOUCH_AVG_DEPTH)   // if average is stable, long delay
            touch_interval_delay();
        else
            touch_next();                   // average isn't ready, get more samples
    }
    else {
        touch_next();
    }
}

void __attribute__((interrupt, shadow, auto_psv)) _T1Interrupt(void) {
    _T1IF = 0;

    _TON = 0;   // disable timer
    _T1IE = 0;  // diable interrupt

    TMR1 = 0;   // reset time period for touch
    PR1 = TOUCH_TIME_CONSTANT;
    _TCKPS = 0;

    touch_next();   // restart sample process
}