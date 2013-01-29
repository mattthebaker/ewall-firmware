#include "ledrow.h"

static unsigned int selected_row = 0;

/** Initialize Row Driver.
 */
void ledrow_init(void) {
    ledrow_disable();
}

/** Switch to a different row.
 *
 * Selects a new row, and enables it.
 * @param The row number to enable.
 */
void ledrow_switch(unsigned int row) {
    selected_row = row;

    ledrow_disable();   // disable active row, to avoid glitches during address
                        // transition

    // update the row address output
    LEDROW_PORT = (LEDROW_PORT & ~LEDROW_PORT_MASK) |
                  ((row << LEDROW_PORT_OFFSET) & LEDROW_PORT_MASK);

    ledrow_enable();    // enable the active row
}

/** Enable Row Drive.
 *
 * The previously enabled row will be turned on.
 */
inline void ledrow_enable(void) {
    if (selected_row / 8)            // enable active row
        LEDROW_PORT_RBEN = 1;
    else
        LEDROW_PORT_RAEN = 1;
}

/** Disable Row Drive.
 */
inline void ledrow_disable(void) {
    LEDROW_PORT_RAEN = 0;   // disable active row
    LEDROW_PORT_RBEN = 0;
}