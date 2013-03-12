/* 
 * File:   touch.h
 * Author: Matt
 *
 * Created on January 29, 2013, 8:55 AM
 */

#ifndef TOUCH_H
#define	TOUCH_H

#ifdef	__cplusplus
extern "C" {
#endif

/** Macro for bit manipulation. Read-ModBit-Write. */
#define RMBITW(var, pos, val) var = (var & ~(1 << (pos))) | ((val % 2) << (pos))

#define TOUCH_CHANNEL_COUNT     22      /**< Number of Touch Channels. */

#define TOUCH_AMUX_AMSEL0       _RB8    /**< uC port for AMSEL0. */
#define TOUCH_AMUX_AMSEL1       _RB9    /**< uC port for AMSEL1. */

/** Touch current pulse duration.  20us -- 320 counts @ 16Mhz FCY */
#define TOUCH_TIME_CONSTANT     160
#define TOUCH_TIME_PRESCALER    0       /**< Current pulse prescaler. 1:1. */

#define TOUCH_SAMPLING_DELAY    19000   /**< Long delay between samples. */
#define TOUCH_DELAY_PRESCALER   1       /**< Long delay prescaler. 8:1. */

#define TOUCH_DETECT_THRESHOLD  100     /**< Touch detection threshold. */
#define TOUCH_AVG_DEPTH         32      /**< Depth of baseline value average. */

#define TOUCH_ADC_PRIORITY      5       /**< ADC interrupt priority. */
#define TOUCH_TIMER_PRIORITY    2       /**< Timer1 interrupt priority. */

void touch_init(void);
void touch_setcallbacks(void (*)(unsigned int), void (*)(unsigned int));
void touch_enable(void);
void touch_disable(void);


#ifdef	__cplusplus
}
#endif

#endif	/* TOUCH_H */

