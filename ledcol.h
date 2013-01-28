/* 
 * File:   ledcol.h
 * Author: Matt
 *
 * Created on January 28, 2013, 8:24 AM
 */

#include <xc.h>

#ifndef LEDCOL_H
#define	LEDCOL_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#define LEDCOL_INT_PRIORITY 3

#define LEDCOL_CMD_CONTROL  0xFF00
#define LEDCOL_CMD_DATA     0x0000

#define LEDCOL_PORT_C_BLANK _RB5
#define LEDCOL_PORT_C0_LAT  _RB4
#define LEDCOL_PORT_C1_LAT  _RC6

#define MAX_CURRENT_R ((int)20.0/35*127)
#define MAX_CURRENT_G ((int)20.0/35*127)
#define MAX_CURRENT_B ((int)20.0/26.3*127)

void ledcol_init(void);
void ledcol_enable(void);
void ledcol_disable(void);
void ledcol_setbrightness(unsigned int, unsigned int, unsigned int);
void ledcol_display(unsigned int [], unsigned int []);
void ledcol_blank(void);
void ledcol_unblank(void);

#ifdef	__cplusplus
}
#endif

#endif	/* LEDCOL_H */

