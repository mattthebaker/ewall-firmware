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

#define RMBITW(var, pos, val) var = (var & ~(1 << (pos))) | ((val % 2) << (pos))

#define TOUCH_CHANNEL_COUNT     22

#define TOUCH_AMUX_AMSEL0       _RB8
#define TOUCH_AMUX_AMSEL1       _RB9

/** 20us: 320 counts @ 16Mhz FCY */
#define TOUCH_TIME_CONSTANT     320

#define TOUCH_SAMPLING_DELAY    19000

#define TOUCH_DETECT_THRESHOLD  100
#define TOUCH_AVG_DEPTH         32

#define TOUCH_CPU_PRIORITY      1

void touch_init(void);
void touch_enable(void);
void touch_disable(void);


#ifdef	__cplusplus
}
#endif

#endif	/* TOUCH_H */

