/* 
 * File:   tmap.h
 * Author: Matt
 *
 * Created on February 3, 2013, 2:16 PM
 */

#ifndef TMAP_H
#define	TMAP_H

#ifdef	__cplusplus
extern "C" {
#endif

#define TOUCHMAP_MAX_HOLDS_PER_CHANNEL      15  /**< Max holds per touch channel. */

#define TOUCHMAP_TIME_CONSTANT              62500   /**< Delay between hold illumination. */
#define TOUCHMAP_TIMER_PRESCALER            3       /**< Delay prescaler. */

/** Struct that represents all holds on a touch channel. */
typedef struct {
    unsigned char count;
    unsigned char holds[TOUCHMAP_MAX_HOLDS_PER_CHANNEL];
} touch_channel;

void touchmap_init(void);
void touchmap_train(void);
void touchmap_gethold(void (*)(unsigned int));
void touchmap_process(void);


#ifdef	__cplusplus
}
#endif

#endif	/* TMAP_H */

