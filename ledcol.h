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

#define LEDCOL_INT_PRIORITY 5           /** LEDCOL interrupt priority. */

#define LEDCOL_CMD_CONTROL  0xFF00      /** TLC5952 control command. */
#define LEDCOL_CMD_DATA     0x0000      /** TLC5952 data command. */

#define LEDCOL_PORT_C_BLANK _RB5        /** uC port for C_BLANK. */
#define LEDCOL_PORT_C0_LAT  _RB4        /** uC port for C0_LAT. */
#define LEDCOL_PORT_C1_LAT  _RC6        /** uC port for C1_LAT. */

#define MAX_CURRENT_R ((int)(20.0/35*127))    /** Max current for R channel, set to 20mA. */
#define MAX_CURRENT_G ((int)(20.0/35*127))    /** Max current for G channel, set to 20mA. */
#define MAX_CURRENT_B ((int)(20.0/26.3*127))  /** Max current for B channel, set to 20mA. */

/** Data packet for TLC5952. */
typedef union {
    struct {
        unsigned :16;
        unsigned :8;
        unsigned char cmdsig;
        unsigned :16;
        unsigned :8;
        unsigned char cmdsig2;
    };
    unsigned char data8[8];
    unsigned int data16[4];
    unsigned long data32[2];
} column_packet;

void ledcol_init(void);
void ledcol_enable(void);
void ledcol_disable(void);
void ledcol_setbrightness(unsigned char, unsigned char, unsigned char);
void ledcol_getbrightness(unsigned char *, unsigned char *, unsigned char *);

void ledcol_display(column_packet *);
void ledcol_clear(void);

void ledcol_blank(void);
void ledcol_unblank(void);

inline void ledcol_bitset_r(column_packet *, unsigned int pos);
inline void ledcol_bitset_g(column_packet *, unsigned int pos);
inline void ledcol_bitset_b(column_packet *, unsigned int pos);

inline void ledcol_bitclr_r(column_packet *, unsigned int pos);
inline void ledcol_bitclr_g(column_packet *, unsigned int pos);
inline void ledcol_bitclr_b(column_packet *, unsigned int pos);

#ifdef	__cplusplus
}
#endif

#endif	/* LEDCOL_H */

