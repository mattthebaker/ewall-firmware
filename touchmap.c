#include <xc.h>
#include <string.h>

#include "touchmap.h"
#include "touch.h"
#include "display.h"
#include "nvm.h"

static void populate_channels(void);
static void touchmap_gethold_presscb(unsigned int);
static void touchmap_gethold_releasecb(unsigned int);
static void touchmap_trainingcb(unsigned int);

static unsigned int touched;            /**< Touch flag for touchmap_train(). */
static unsigned int touched_channel;    /**< Channel that was touched. */

static touch_channel channels[TOUCH_CHANNEL_COUNT]; /**< Touch channel -> Hold map. */

typedef enum {STATE_IDLE, STATE_UNTOUCHED, STATE_TOUCHED} gethold_states;

static gethold_states gethold_state;    /**< State of gethold state machine.*/
static unsigned int gethold_channel;    /**< Channel that was touched. */
static unsigned int gethold_index;      /**< Index of hold that is displayed. */
static unsigned int gethold_touchflag;  /**< Flag that a channel was touched. */
static unsigned int gethold_releaseflag;    /**< Flag that the channel was released. */
static route gethold_route;             /**< Route used to display the holds. */
static void (*gethold_callback)(unsigned int);  /**< Function to call when gethold finishes. */

/** Initialize touchmap module. */
void touchmap_init(void) {
    populate_channels();

    T4CONbits.T32 = 0;
    T4CONbits.TCKPS = TOUCHMAP_TIMER_PRESCALER;
    PR4 = 0xffff;
    TMR4 = 0;

    gethold_state = STATE_IDLE;
}

/** Populate touch_channel -> hold map. */
static void populate_channels(void) {
    const __psv__ unsigned char *nvmdata;

    memset(channels, 0, sizeof(channels));
    if (nvm_valid()) {      // populate touch_channel map
        int i;
        nvmdata = (const __psv__ unsigned char *) nvm_read(0);
        for (i = 0; i < DISPLAY_COLS*DISPLAY_ROWS; i++) {
            touch_channel *temp = &channels[nvmdata[i]];
            temp->holds[temp->count++] = i;
        }
    }
}

/** Press event callback for gethold. */
static void touchmap_gethold_presscb(unsigned int chan) {
    if (!gethold_touchflag) {
        gethold_touchflag = 1;
        gethold_channel = chan;
    }
}

/** Release event callback for gethold. */
static void touchmap_gethold_releasecb(unsigned int chan) {
    if (chan == gethold_channel)
        gethold_releaseflag = 1;
}

/** Starts the state machine to get a hold from the user.
 * Upon touch, the routine lights each hold on the channel in sequence, until
 * the user releases the touch.  Once the user has chosen the hold, the
 * callback will be called.  touchmap_process() should be called periodically
 * until the callback has been called.
 * @param cb Function to call when once the hold is selected.
 */
void touchmap_gethold(void (*cb)(unsigned int)) {
    gethold_route.id = 254;
    gethold_route.len = 1;
    gethold_route.r = 255;
    gethold_route.g = 255;
    gethold_route.b = 255;
    
    gethold_channel = 0;
    gethold_touchflag = 0;
    gethold_releaseflag = 0;

    gethold_callback = cb;

    gethold_state = STATE_UNTOUCHED;
    touch_setcallbacks(touchmap_gethold_presscb, touchmap_gethold_releasecb);
    touch_enable();
}

/** Process the state machine that implements touchmap_gethold().
 * The state machine will run until the user selects a hold.  This function
 * can be called when gethold is inactive.
 */
void touchmap_process(void) {
    switch (gethold_state) {
        case STATE_IDLE:
            break;

        case STATE_UNTOUCHED:           // wait for touch to identify channel
            if (gethold_touchflag) {
                gethold_index = 0;

                // display first hold on the channel
                gethold_route.holds[0] = channels[gethold_channel].holds[gethold_index];
                display_showroute(&gethold_route);

                TMR4 = 0;
                T4CONbits.TON = 1;
                gethold_state = STATE_TOUCHED;
            }
            break;

        case STATE_TOUCHED:
            if (gethold_releaseflag) {  // if the user releases, we have our hold
                T4CONbits.TON = 0;
                TMR4 = 0;

                // signal our user that we received the hold
                gethold_callback(channels[gethold_channel].holds[gethold_index]);
                gethold_state = STATE_IDLE;
            } else if (TMR4 > TOUCHMAP_TIME_CONSTANT) { // timer timeout, display next hold
                TMR4 = 0;
                display_hideroute(gethold_route.id);
                gethold_index++;
                gethold_index %= channels[gethold_channel].count;
                gethold_route.holds[0] = channels[gethold_channel].holds[gethold_index];
                display_showroute(&gethold_route);
            }
            break;

        default:
            gethold_state = STATE_IDLE;
    }
}

/** Touch callback used during training routine. */
static void touchmap_trainingcb(unsigned int chan) {
    touched = 1;
    touched_channel = chan;
}

/** Touch Map Training.
 * Solicits touch input from the user to map every hold on the wall to a touch
 * channel.  After completing the map, the data is written to NVM.
 */
void touchmap_train(void) {
    route disphold;
    unsigned int i;
    unsigned int tchan;
    unsigned char map[DISPLAY_ROWS][DISPLAY_COLS];

    disphold.id = 254;  // route to display the holds.
    disphold.len = 1;
    disphold.r = 255;
    disphold.g = 255;
    disphold.b = 255;

    touched = 0;

    touch_setcallbacks(touchmap_trainingcb, (0));
    touch_enable();

    for (i = 0; i < DISPLAY_COLS * DISPLAY_ROWS; i++) { // loop through the holds
        disphold.holds[0] = i;

        // TODO: do we need to enable/disable display when changing routes?
        display_showroute(&disphold);

        while (!touched)    // wait for input
            display_process();

        __builtin_disi(0x3fff);
        tchan = touched_channel;
        touched = 0;
        __builtin_disi(0);

        map[i >> 4][i & 0xF] = tchan;

        display_hideroute(disphold.id);
    }

    nvm_program(sizeof(map) >> 1, 0, (unsigned int *)map);

    populate_channels();
}

