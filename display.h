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

#define DISPLAY_SCAN_FREQ       60

#define DISPLAY_COLOR_DEPTH     32

#define DISPLAY_ROUTE_LEN       20
#define DISPLAY_MAX_ROUTES      8

#define DISPLAY_FIFO_LEN        32

#define DISPLAY_ROWS            16
#define DISPLAY_COLS            16

typedef struct {
    unsigned char id;
    unsigned char heartbeat;
    unsigned char len;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char holds[DISPLAY_ROUTE_LEN];
} route;

typedef struct {
    unsigned char row;
    unsigned char repeat;
    column_data cdata;
} display_data;

void display_init(void);
void display_enable(void);
void display_disable(void);
void display_process(void);

void display_showroute(route *);
void display_hideroute(unsigned int);
void display_clear(void);


void fifo_get(display_data *);
void fifo_put(display_data *);
int fifo_full(void);
int fifo_empty(void);



#ifdef	__cplusplus
}
#endif

#endif	/* DISPLAY_H */

