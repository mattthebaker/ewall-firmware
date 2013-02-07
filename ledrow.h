/* 
 * File:   ledrow.h
 * Author: Matt
 *
 * Created on January 28, 2013, 1:32 PM
 */

#include <xc.h>

#ifndef LEDROW_H
#define	LEDROW_H

#ifdef	__cplusplus
extern "C" {
#endif

#define LEDROW_PORT_RAEN    _RA10   /**< uC port for RAEN.*/
#define LEDROW_PORT_RBEN    _RA4    /**< uC port for RBEN.*/
    
#define LEDROW_ADDR_PORT    LATA    /**< uC port for Row address.*/
#define LEDROW_ADDR_MASK    0x380   /**< Row address mask.*/
#define LEDROW_ADDR_OFFSET  7       /**< Row address offset.*/

void ledrow_init(void);
void ledrow_switch(unsigned int);
void ledrow_enable(void);
void ledrow_disable(void);

#ifdef	__cplusplus
}
#endif

#endif	/* LEDROW_H */

