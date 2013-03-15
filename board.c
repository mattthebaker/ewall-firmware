#include <xc.h>

#include "board.h"

/** Set configuration bits.
 *
 * Sets up Oscillator, Debug port, etc.
 */
_CONFIG1(JTAGEN_OFF & GCP_OFF & GWRP_OFF & ICS_PGx1 & FWDTEN_OFF);
_CONFIG2(IESO_OFF & PLLDIV_NODIV & PLL96MHZ_ON & FNOSC_FRCPLL & 
         FCKSM_CSDCMD & OSCIOFNC_ON & IOL1WAY_OFF & POSCMOD_NONE);
_CONFIG3(WPCFG_WPCFGDIS & WPDIS_WPDIS & WUTSEL_LEG & SOSCSEL_IO);
_CONFIG4(DSWDTEN_OFF & DSBOREN_OFF);

/** Initialize the uC with board specific settings.
 *
 * Configure output ports, and peripheral pin switch.
 */
void board_init(void) {
    // set pins that are output as output
    TRISA = TRISA & ~0x079f;    // A: 0-4,7-10
    TRISB = TRISB & ~0x733c;    // B: 2-5,8-9,13-15
    TRISC = TRISC & ~0x01ff;    // C: 0-8

    // map SPI modules using peripheral pin select
    __builtin_write_OSCCONL(OSCCON & ~_OSCCON_IOLOCK_MASK); // unlock pin select
    _SDI1R = 7;     // SPI1 data input from RP7
    _SDI2R = 25;    // SPI2 data input from RP25
    _RP23R = 11;   // SPI2 clock output to RP23
    _RP24R = 10;   // SPI2 data output to RP24
    _RP20R = 8;    // SPI1 clock output to RP20
    _RP21R = 7;    // SPI1 data output to RP21
    __builtin_write_OSCCONL(OSCCON | _OSCCON_IOLOCK_MASK);  // lock pin select

    _USB1IP = USB_INT_PRIORITY;

}