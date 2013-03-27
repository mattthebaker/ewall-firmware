#include "ledrow.h"

static unsigned int selected_row;   /**< The currently selected row. */

/** Initialize Row Drive Module.
 */
void ledrow_init(void) {
    selected_row = 0;
    ledrow_disable();
}

/** Switch to a different row.
 *
 * Selects a new row, and enables it.
 * @param row The row number to enable.
 */
void ledrow_switch(unsigned int row) {
    selected_row = row; // set selected row

    ledrow_disable();   // disable active row, to avoid glitches during address
                        // transition

    // update the row address output
    LEDROW_ADDR_PORT = (LEDROW_ADDR_PORT & ~LEDROW_ADDR_MASK) |
                  ((row << LEDROW_ADDR_OFFSET) & LEDROW_ADDR_MASK);

    ledrow_enable();    // enable the active row
}

/** Enable Row Drive.
 *
 * The previously enabled row will be turned on.
 */
inline void ledrow_enable(void) {
    LEDROW_PORT_RBEN = 1;
    LEDROW_PORT_RAEN = 1;
}

/** Disable Row Drive.
 */
inline void ledrow_disable(void) {
    LEDROW_PORT_RAEN = 0;   // disable active rows
    LEDROW_PORT_RBEN = 0;
}