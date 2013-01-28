#include "ledrow.h"

static unsigned int selected_row = 0;

void ledrow_init(void) {
    ledrow_disable();
}

void ledrow_switch(unsigned int row) {
    selected_row = row;

    ledrow_disable();   // disable active row, to avoid glitches during address
                        // transition

    LEDROW_PORT = (LEDROW_PORT & ~LEDROW_PORT_MASK) |
                  ((row << LEDROW_PORT_OFFSET) & LEDROW_PORT_MASK);

    ledrow_enable();
}

inline void ledrow_enable(void) {
    if (selected_row / 8)            // enable active row
        LEDROW_PORT_RBEN = 1;
    else
        LEDROW_PORT_RAEN = 1;
}

inline void ledrow_disable(void) {
    LEDROW_PORT_RAEN = 0;   // disable active row
    LEDROW_PORT_RBEN = 0;
}