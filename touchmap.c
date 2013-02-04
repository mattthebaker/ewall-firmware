#include <xc.h>
#include <string.h>

#include "touchmap.h"
#include "touch.h"
#include "display.h"
#include "nvm.h"

unsigned int touched;
unsigned int touched_channel;

touch_channel channels[TOUCH_CHANNEL_COUNT];

typedef enum {STATE_IDLE, STATE_UNTOUCHED, STATE_TOUCHED} gethold_states;

gethold_states gethold_state;
unsigned int gethold_channel;
unsigned int gethold_index;
unsigned int gethold_touchflag;
unsigned int gethold_releaseflag;
route gethold_route;
void (*gethold_callback)(unsigned int);

void touchmap_init(void) {
    const __psv__ unsigned char *nvmdata;

    touched = 0;

    memset(channels, 0, sizeof(channels));
    
    if (nvm_valid()) {
        int i;
        nvmdata = (const __psv__ unsigned char *) nvm_read(0);
        for (i = 0; i < DISPLAY_COLS*DISPLAY_ROWS; i++) {
            touch_channel *temp = &channels[nvmdata[i]];
            temp->holds[temp->count++] = i;
        }
    }

    T4CONbits.T32 = 0;
    T4CONbits.TCKPS = TOUCHMAP_TIMER_PRESCALER;
    PR4 = 0xffff;
    TMR4 = 0;

    gethold_state = STATE_IDLE;
}


void touchmap_gethold_presscb(unsigned int chan) {
    if (!gethold_touchflag) {
        gethold_touchflag = 1;
        gethold_channel = chan;
    }
}

void touchmap_gethold_releasecb(unsigned int chan) {
    if (chan == gethold_channel)
        gethold_releaseflag = 1;
}

void touchmap_gethold(void (*cb)(unsigned int)) {
    gethold_route.id = 254;
    gethold_route.len = 1;
    gethold_route.r = 255;
    gethold_route.g = 255;
    gethold_route.b = 255;
    
    gethold_channel = 0;
    gethold_touchflag = 0;

    gethold_callback = cb;

    gethold_state = STATE_UNTOUCHED;
    touch_setcallbacks(touchmap_gethold_presscb, touchmap_gethold_releasecb);
    touch_enable();
}

void touchmap_process(void) {
    switch (gethold_state) {
        case STATE_IDLE:
            break;

        case STATE_UNTOUCHED:
            if (gethold_touchflag) {
                gethold_index = 0;
                gethold_route.holds[0] = channels[gethold_channel].holds[gethold_index];
                display_showroute(&gethold_route);

                TMR4 = 0;
                T4CONbits.TON = 1;
                gethold_state = STATE_TOUCHED;
            }
            break;

        case STATE_TOUCHED:
            if (gethold_releaseflag) {
                T4CONbits.TON = 0;
                TMR4 = 0;
                gethold_callback(channels[gethold_channel].holds[gethold_index]);
                gethold_state = STATE_IDLE;
            } else if (TMR4 > TOUCHMAP_TIME_CONSTANT) {
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

void touchmap_trainingcb(unsigned int chan) {
    __builtin_disi(0x3fff);
    touched = 1;
    touched_channel = chan;
    __builtin_disi(0);
}

void touchmap_train(void) {
    route disphold;
    unsigned int i;
    unsigned int tchan;
    unsigned char map[DISPLAY_ROWS][DISPLAY_COLS];

    disphold.id = 254;
    disphold.len = 1;
    disphold.r = 255;
    disphold.g = 255;
    disphold.b = 255;

    touched = 0;

    touch_setcallbacks(touchmap_trainingcb, (0));
    touch_enable();

    for (i = 0; i < DISPLAY_COLS * DISPLAY_ROWS; i++) {
        disphold.holds[0] = i;

        // TODO: do we need to enable/disable display when changing routes?
        display_showroute(&disphold);

        while (!touched)
            display_process();

        __builtin_disi(0x3fff);
        tchan = touched_channel;
        touched = 0;
        __builtin_disi(0);

        map[i >> 4][i & 0xF] = tchan;

        display_hideroute(disphold.id);
    }

    nvm_program(sizeof(map) >> 1, 0, (unsigned int *)map);
}

