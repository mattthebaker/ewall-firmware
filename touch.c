#include <xc.h>
#include <stddef.h>

#include "touch.h"

/** Struct that holds touch channel information. */
typedef struct {
    unsigned port:2;
    unsigned pindex:4;
    unsigned amux:2;
    unsigned aindex:4;
} touch_channel;

static void filter_reset(void);
static void touch_nextsample(void);
static void touch_nextchannel(void);
static void touch_interval_delay(void);
static void touch_process_samples(void);
static void touch_process_rc(void);

/** Const array that contains touch channel->peripheral mapping information. */
static const touch_channel cmatrix[TOUCH_CHANNEL_COUNT] =   {{1, 2, 0, 4},
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
                                                            {2, 1, 0, 7},
                                                            {1, 3, 2, 5},
                                                            {1, 3, 0, 5},
                                                            {1, 3, 3, 5},
                                                            {1, 3, 1, 5},
                                                            {2, 0, 2, 6},
                                                            {2, 0, 1, 6},
                                                            {2, 0, 0, 6},
                                                            {2, 0, 3, 6}};

/** Array that maps a PORT index to the register. */
static volatile unsigned int * const ports[3] = {&PORTA, &PORTB, &PORTC};

/** Array that maps a TRIS index to the register. */
static volatile unsigned int * const tris[3] = {&TRISA, &TRISB, &TRISC};

static unsigned int enabled = 0;        /**< Touch enabled flag. */
static unsigned int shutting_down = 0;  /**< Shutdown process flag. */
static unsigned int active_channel = 0; /**< Active touch channel. */
static unsigned int long_delay = 0;     /**< Long delay flag. */
static unsigned int short_delay = 0;
static unsigned int delay = 0;

static unsigned int touch_process_flag;       /**< Samples need processing Flag.*/

static unsigned int avg_depth;      /**< Depth of accumulated average. */
static unsigned int samples[TOUCH_CHANNEL_COUNT];   /**< Most recent samples. */
static unsigned int p2samples[TOUCH_CHANNEL_COUNT];
static unsigned int sampleavg[TOUCH_CHANNEL_COUNT];
static unsigned int basecount[TOUCH_CHANNEL_COUNT]; /**< Base capacitance count. */
static unsigned int p2basecount[TOUCH_CHANNEL_COUNT];
static unsigned int threshold_count[TOUCH_CHANNEL_COUNT];   /**< Count of consecutive threshold triggers. */
static unsigned int subthreshold_count[TOUCH_CHANNEL_COUNT];
static unsigned int sample_count;
static unsigned int temp_samples[6];
static unsigned int temp_depth;

static touch_channel prev_chan;
static touch_channel cur_chan;

static unsigned int rc_detect;
static unsigned int rc_samples;
static unsigned int rc_i;
static unsigned int rc_avgs[2];

static void (*press_cb)(unsigned int);  /**< Press event callback pointer. */
static void (*release_cb)(unsigned int);    /**< Release event callback pointer. */

/** Initialize touch sensing.
 * Configure Timer1, and the ADC for touch sensing.
 */
void touch_init(void) {
    TMR1 = 0;                   // configure timer
    PR1 = TOUCH_TIME_CONSTANT;
    _TCKPS = TOUCH_TIME_PRESCALER;
    _T1IP = TOUCH_TIMER_PRIORITY;
    _T1IE = 0;

    // CTMU configuration
    _CTMUEN = 0;                // disable
    _EDGSEQEN = 1;              // require two edges for current pulse
    _EDG2POL = 1;               // rising edge
    _EDG1POL = 1;
    _EDG1SEL = 1;               // TODO: Is this right? Want to manually set this edge.
    _CTTRIG = 0;                // trigger A/D conversion when current shuts off
    _IRNG = 3;                  // 5.5 uA
    _IDISSEN = 1;               // ground current source
    _EDG2STAT = 0;              // clear edge status
    _EDG1STAT = 0;
    _CTMUIF = 0;
    _CTMUIP = TOUCH_CTMU_PRIORITY;

    // ADC configuration
    _SSRC = 0;                  // CTMU triggers conversion
    _CH0SA = 1;                 // set default input to a grounded touch channel
    _ADON = 0;
    _ADCS = 5; //39;                 // tAD = tCY(ADCS + 1); tCY = tOSC/2
    _AD1IP = TOUCH_ADC_PRIORITY;    // TODO: determine proper interrupt priority, should be low prio
    _SMPI = 0;
    _SAMC = 0;

    press_cb = NULL;
    release_cb = NULL;

    touch_process_flag = 0;
}

/** Register touch event callbacks.
 *
 * Callbacks should execute quickly, as they are called from interrupts.
 * @param press Press event callback.
 * @param release Release event callback.
 */
void touch_setcallbacks(void (*press)(unsigned int), void (*release)(unsigned int)) {
    press_cb = press;
    release_cb = release;
}

/** Enable touch sensing.
 * Configures device for touch sensing, and starts the sense on the first
 * channel.  Every time the touch module is disabled and re-enabled, it must
 * reaquire the baseline average. Avoid disabling the module unless it will
 * be unused for a long period of time.
 */
void touch_enable(void) {
    int i;

    if (enabled)        // allow this function to be called multiple times.
        return;

    __builtin_disi(0x3fff);     // interrupt shutdown if re-enabled
    if (shutting_down) {
        enabled = 1;
        shutting_down = 0;
        __builtin_disi(0);
        return;
    }
    __builtin_disi(0);

    _CTMUEN = 1;                // enable CTMU
    _EDG2STAT = 0;              // clear edge status
    _EDG1STAT = 0;
    _EDGEN = 1;                 // enable edge inputs
    _CTMUIF = 0;
    _CTMUIE = 0;

    _ADON = 1;                  // enable ADC
    _AD1IF = 0;
    _AD1IE = 1;                 // enable interrupts
    _ASAM = 0;

    _T1IE = 1;

    filter_reset();

    enabled = 1;
    active_channel = TOUCH_CHANNEL_COUNT - 1;

    temp_depth = 0;
    temp_samples[0] = 0;
    temp_samples[1] = 0;
    temp_samples[2] = 0;
    temp_samples[3] = 0;
    temp_samples[4] = 0;
    temp_samples[5] = 0;

    delay = long_delay = short_delay = 0;
    rc_detect = 0;

    touch_nextchannel();
}

/** Disable touch sensing.
 */
void touch_disable(void) {
    if (!enabled)
        return;
    
    __builtin_disi(0x3fff);
    if (long_delay) {   // in long delay, immediately disable
        _TON = 0;
        _T1IF = 0;
        TMR1 = 0;                      // reset time period for touch
        PR1 = TOUCH_TIME_CONSTANT;
        _TCKPS = TOUCH_TIME_PRESCALER; // reset prescaler
        delay = 0;
        long_delay = 0;
    } else {
        shutting_down = 1;  // sense is taking place, trigger shutdown
    }
    enabled = 0;
    __builtin_disi(0);
}

static void touch_nextchannel(void) {
    int i;

    sample_count = 0;

    if (!rc_detect) {
        prev_chan = cmatrix[active_channel];
        //prev_chan = cmatrix[0];

        active_channel = (active_channel + 1) % TOUCH_CHANNEL_COUNT;

        //active_channel = 0;
        cur_chan = cmatrix[active_channel];
    }

    RMBITW(*(tris[prev_chan.port]), prev_chan.pindex, 0);   // ground inactive channel
    RMBITW(*(ports[prev_chan.port]), prev_chan.pindex, 0);  // TODO: shouldn't need this (value should always be 0).

    TOUCH_AMUX_AMSEL0 = cur_chan.amux % 2;    // switch amux
    TOUCH_AMUX_AMSEL1 = cur_chan.amux / 2;

    if (temp_depth == 32)
        Nop();

    _CH0SA = cur_chan.aindex;   // switch ADC channel
    _SAMP = 1;

//    if (rc_detect) {
        delay = short_delay = 1;
        TMR1 = 0;       // reset timer count
        _TCKPS = TOUCH_DISCHARGE_PRESCALER;
        PR1 = TOUCH_DISCHARGE_DELAY;
        _TON = 1;
        return;
//    }

//    touch_nextsample();
}

/** Start the sense on the next touch channel.
 * Configures the next channel, and starts a sense.
 */
static void touch_nextsample(void) {
    RMBITW(*(tris[cur_chan.port]), cur_chan.pindex, 1);     // set pin to input

    TMR1 = 0;       // reset timer count
    PR1 = TOUCH_TIME_CONSTANT;
    _TCKPS = TOUCH_TIME_PRESCALER;
    _IDISSEN = 0;   // disable current source override grounder

    __builtin_disi(0x3fff); // disable interrupts for critical timing
    _EDG1STAT = 1;          // enable current source
    _TON = 1;               // enable timer
    __builtin_disi(0);      // re-enable interrupts
}

/** Long delay function to spread sampling over time.
 * The delay is implemented by reconfiguring Timer1, and then waiting for
 * an interrupt.
 */
static void touch_interval_delay(void) {
    TMR1 = 0;
    _TCKPS = TOUCH_DELAY_PRESCALER;     // 8:1 prescaler
    PR1 = TOUCH_SAMPLING_DELAY;

    delay = long_delay = 1;
    _T1IF = 0;
    _T1IE = 1;
    _TON = 1;
}

void touch_process(void) {
    if (!enabled || !touch_process_flag)
        return;

    touch_process_flag = 0;

    if (long_delay) {
        delay = long_delay = 0;
        touch_nextchannel();
        return;
    }

    touch_process_samples();
            
    if (avg_depth == TOUCH_AVG_DEPTH)   // if average is stable, long delay
        touch_interval_delay();
    else
        touch_nextchannel();                   // average isn't ready, get more samples
}

/** Process a batch of touch samples.
 * This function handles touch detection, calls callbacks, and maintains the
 * base level average.  It should be called each time a batch of samples has
 * been taken.
 */
static void touch_process_samples(void) {
    unsigned int thresh_exceeded = 0;
    unsigned int i;
    unsigned int avg, savg;

    if (avg_depth < TOUCH_AVG_DEPTH) {      // accumulate baseline before detecting touch
        for (i = 0; i < TOUCH_CHANNEL_COUNT; i++) {
            basecount[i] += samples[i];
            p2basecount[i] += p2samples[i];
            sampleavg[i] += samples[i];
        }
        avg_depth++;
        return;
    }

    for (i = 0; i < TOUCH_CHANNEL_COUNT; i++) {     // touch detection
        sampleavg[i] += (samples[i] << 2) - sampleavg[i] / (avg_depth >> 2);
        savg = sampleavg[i] / avg_depth;
        avg = basecount[i] / TOUCH_AVG_DEPTH;
        if (avg > savg && (avg - savg) > TOUCH_DETECT_THRESHOLD) {
            thresh_exceeded++;
            threshold_count[i]++;
            if (threshold_count[i] == TOUCH_HYST_COUNT) {  // soft debounc
                rc_detect = 1;
                //if (press_cb)               // if touched, callback
                    //press_cb(i);
            }
            subthreshold_count[i] = 0;
        }
        else
            if (threshold_count[i] >= TOUCH_HYST_COUNT) {   // if released, callback
                subthreshold_count[i]++;
                if (subthreshold_count[i] >= TOUCH_HYST_COUNT) {
                    if (release_cb)
                        release_cb(i);
                    threshold_count[i] = 0;
                }
            } else
                threshold_count[i] = 0;
    }

    if (!thresh_exceeded)   // if no finger is near, apply current sample to average
        for (i = 0; i < TOUCH_CHANNEL_COUNT; i++) {
            if (samples[i] > basecount[i] / avg_depth)
                basecount[i]++;
            else
                basecount[i]--;
            
            if (p2samples[i] > p2basecount[i] / avg_depth)
                p2basecount[i]++;
            else
                p2basecount[i]--;
        }

    if (rc_detect)
        touch_process_rc();
}

static void touch_process_rc(void) {
    static const unsigned char vpercent[] = {100*TOUCH_RC_1, 100*TOUCH_RC_2, 100*TOUCH_RC_3, 100*TOUCH_RC_4, 100*TOUCH_RC_5, 100*TOUCH_RC_6};
    int i;

    if (rc_samples == 64) {
        unsigned int center;
        unsigned int window = (p2basecount[rc_i] - rc_avgs[1]/2)/32;
        unsigned w_width = window*((unsigned int)(100*TOUCH_RC_W))/100;
        unsigned int rc_val = (rc_avgs[0] - rc_avgs[1])/64;

        for (i = 0; i < 6; i++) {
            center = window*(vpercent[i])/100;
            if (rc_val >= (center - w_width) && rc_val <= (center + w_width))
                if (press_cb)
                    press_cb(i << 5 | rc_i);
        }

        rc_i++;
    }

    for (rc_i; rc_i < TOUCH_CHANNEL_COUNT; rc_i++)
        if (threshold_count[rc_i] == TOUCH_HYST_COUNT) {
            rc_samples = 0;
            rc_avgs[0] = 0;
            rc_avgs[1] = 0;
            cur_chan = cmatrix[rc_i];
            touch_nextchannel();
            break;
        }

    if (rc_i >= TOUCH_CHANNEL_COUNT) {
        rc_i = 0;
        rc_samples = 0;
        rc_detect = 0;
        touch_nextchannel();
    }
}

/** Reset touch filter.
 */
static void filter_reset(void) {
    int i;
    
    avg_depth = 0;
    for (i = 0; i < TOUCH_CHANNEL_COUNT; i++) {
        samples[i] = 0;
        sampleavg[i] = 0;
        p2samples[i] = 0;
        basecount[i] = 0;
        p2basecount[i] = 0;
        threshold_count[i] = 0;
        subthreshold_count[i] = 0;
    }
}

/** ADC Interrupt Service Routine.
 * Handles the completion of a touch sense.  The ADC is automatically triggered
 * by the CTMU after the current pulse.  This handler stores the result in the
 * sample buffer.  After every channel has been sampled, it will call the
 * processing handler.  If we have a stable baseline, it will trigger a long
 * delay, otherwise it will start the next sample.
 */
void __attribute__((interrupt, auto_psv)) _ADC1Interrupt(void) {
    _AD1IF = 0;

    if (sample_count < 5) {
        _SAMP = 1;
        temp_samples[sample_count++] = ADC1BUF0;
        Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop(); Nop();
        _SAMP = 0;
        return;
    }
//    _SAMP = 0;

    _IDISSEN = 1;

    _EDG2STAT = 0;
    _EDG1STAT = 0;

    //SET_CPU_IPL(0);
    temp_samples[sample_count] += ADC1BUF0;
    temp_depth++;

    samples[active_channel] = ADC1BUF0;
    p2samples[active_channel] = temp_samples[1];

    if (rc_detect) {
//        unsigned int avg = basecount[rc_i] / TOUCH_AVG_DEPTH;
//        if (avg > samples[rc_i] && (avg - samples[rc_i]) > TOUCH_DETECT_THRESHOLD) {
            rc_avgs[0] += temp_samples[1];
            rc_avgs[1] += ADC1BUF0;
            rc_samples++;
 //       }
    }

    if (shutting_down) {    // shutdown (break interrupt loop)
        shutting_down = 0;
        return;
    }

    if (rc_detect) {
        if (rc_samples == 64)
            touch_process_rc();
        else
            touch_nextchannel();
        return;
    }

    if (active_channel == TOUCH_CHANNEL_COUNT - 1)  // sampled every channel
        touch_process_flag = 1;
    else 
        touch_nextchannel();
}

void __attribute__((interrupt, auto_psv)) _CTMUInterrupt(void) {
    _CTMUIF = 0;
//    _SAMP = 0;
}

/** Timer1 Interrupt Service Routine.
 * Handles the end of the long sampling delay.  It shuts off the timer,
 * and reconfigures it for generating the touch current pulse.
 */
void __attribute__((interrupt, auto_psv)) _T1Interrupt(void) {
    _T1IF = 0;
    _TON = 0;   // disable timer

    if (!delay) {
//        _EDG2STAT = 0;
//        _EDG1STAT = 0;
        _SAMP = 0;
        return;
    }

    if (short_delay) {
        delay = short_delay = 0;
        touch_nextsample();
        return;
    }

//    _T1IE = 0;
//    long_delay = 0;
    touch_process_flag = 1;
    TMR1 = 0;   // reset time period for touch
    PR1 = TOUCH_TIME_CONSTANT;
    _TCKPS = TOUCH_TIME_PRESCALER; // reset prescaler

//    touch_next();   // restart sample process
}