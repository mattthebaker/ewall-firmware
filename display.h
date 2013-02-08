/* 
 * File:   display.h
 * Author: Matt
 *
 * Created on January 31, 2013, 10:21 AM
 */

#include "ledcol.h"

#ifndef DISPLAY_H
#define	DISPLAY_H

#ifdef	__cplusplus
extern "C" {
#endif

#define CLOCK_FREQUENCY         16000000    /**< Clock Frequency. */
#define DISPLAY_SCAN_FREQ       60          /**< Display updates per second. */

#define DISPLAY_HEARTBEAT_PERIOD    5       /**< Duration of heartbeat pulse. */

#define DISPLAY_COLOR_DEPTH     32          /**< Display color depth. */

#define DISPLAY_ROUTE_LEN       20          /**< Maximum route length. */
#define DISPLAY_MAX_ROUTES      8           /**< Maximum active routes. */

#define DISPLAY_FIFO_LEN        32          /**< Size of display output FIFO. */

#define DISPLAY_ROWS            16          /**< Number of Rows in display. */
#define DISPLAY_COLS            16          /**< Number of Cols in display. */

#define MAX(a,b,c) ((a > b)? a: ((b > c)? b : c))   /**< Max of three macro. */

/** Structure that stores a route for display. */
typedef struct {
    unsigned char id;           /**< Route identifier. Used to deactive a route. */
    unsigned char heartbeat;    /**< Flag for heartbeat mode. */
    unsigned char len;          /**< Hold count in the route. */
    unsigned char r;            /**< Red color channel. */
    unsigned char g;            /**< Green color channel. */
    unsigned char b;            /**< Blue color channel. */
    unsigned char holds[DISPLAY_ROUTE_LEN]; /**< Array of hold identifiers. */
} route;

/** Structure that stores an active row for display.
 * Used internally to represent the state of the display.  It is faster and
 * more intuitive to operate on rows than on routes. */
typedef struct {
    unsigned char enabled;          /**< Fast flag if any holds ont he row are active. */
    unsigned char maxbrightness;    /**< Max brightness in the row. */
    route *holds[DISPLAY_COLS];     /**< Pointer back to a route structure for each hold. */
} row;

/** Display data to send to the row/column drivers.
 * This is passed from the display module to the line drivers. */
typedef struct {
    unsigned char row;      /**< Row that should be activated. */
    unsigned char repeat;   /**< How many periods to repeat this pattern. */
    column_packet cdata;    /**< Raw data to send to the column drivers. */
} display_data;

void display_init(void);
void display_enable(void);
void display_disable(void);
void display_process(void);

void display_showroute(route *);
void display_hideroute(unsigned int);
void display_clearroutes(void);


#ifdef	__cplusplus
}
#endif

#endif	/* DISPLAY_H */

