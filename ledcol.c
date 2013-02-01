#include <xc.h>

#include "ledcol.h"
#include "touch.h"

/* 
To set up the SPI module for the Enhanced Buffer Master mode of operation:
1. If using interrupts:
    a) Clear the SPIxIF bit in the respective IFS register.
    b) Set the SPIxIE bit in the respective IEC register.
    c) Write the SPIxIP bits in the respective IPC register.
2. Write the desired settings to the SPIxCON1 and
    SPIxCON2 registers with MSTEN (SPIxCON1<5>) = 1.
3. Clear the SPIROV bit (SPIxSTAT<6>).
4. Select Enhanced Buffer mode by setting the SPIBEN bit (SPIxCON2<0>).
5. Enable SPI operation by setting the SPIEN bit (SPIxSTAT<15>).
6. Write the data to be transmitted to the SPIxBUF register. Transmission
  (and reception) will start as soon as
*/

static unsigned int gbright_r = MAX_CURRENT_R;
static unsigned int gbright_g = MAX_CURRENT_G;
static unsigned int gbright_b = MAX_CURRENT_B;

/**
 * Initialize the LED Column driver.
 *
 * Configures SPI1 and SPI2 modules for interfacing with the TLC5952 LED
 * Driver chip.
 */
void ledcol_init(void) {
    // SPI1 Initialization
    _SPI1IF = 0;
    _SPI1IE = 1;
    _SPI1IP = LEDCOL_INT_PRIORITY;

    SPI1CON1bits.MODE16 = 1;    // use 16-bit transmit mode
    SPI1CON1bits.CKE = 1;       // data transition on falling clock edge
    SPI1CON1bits.MSTEN = 1;     // master mode
    SPI1CON1bits.SPRE = 6;      // secondary prescale: 2:1
    SPI1CON1bits.PPRE = 3;      // primary prescale: 1:1 (8Mhz)

    SPI1CON2bits.SPIBEN = 1;    // enhanced buffer mode

    SPI1STATbits.SPIROV = 0;    // clear overflow flag
    SPI1STATbits.SISEL = 5;     // interupt when transmission completes

    // SPI2 Initialization
    _SPI2IF = 0;
    _SPI2IE = 1;
    _SPI2IP = LEDCOL_INT_PRIORITY;

    SPI2CON1bits.MODE16 = 1;    // use 16-bit transmit mode
    SPI2CON1bits.CKE = 1;       // data transition on falling clock edge
    SPI2CON1bits.MSTEN = 1;     // master mode
    SPI2CON1bits.SPRE = 6;      // secondary prescale: 2:1
    SPI2CON1bits.PPRE = 3;      // primary prescale: 1:1 (8Mhz)

    SPI2CON2bits.SPIBEN = 1;    // enhanced buffer mode

    SPI2STATbits.SPIROV = 0;    // clear overflow flag
    SPI2STATbits.SISEL = 5;     // interupt when transmission completes

    ledcol_enable();
    ledcol_setbrightness(MAX_CURRENT_R, MAX_CURRENT_G, MAX_CURRENT_B);
    ledcol_disable();
}

/** Enable the SPI channels on the LED Column Driver.
 */
void ledcol_enable(void) {
    SPI1STATbits.SPIEN = 1;
    SPI2STATbits.SPIEN = 1;
}

/** Disable the SPI channels for the LED Column Driver.
 */
void ledcol_disable(void) {
    SPI1STATbits.SPIEN = 0;
    SPI2STATbits.SPIEN = 0;
}

/** Set the LED brightness.
 *
 * Sets the value of the current source in the LED driver.  Use this balance
 * the brightness between the three color channels.  Be cautious not to
 * exceed the current rating of the LEDs.
 *
 * @param Red Channel Brightness
 * @param Green Channel Brightness
 * @param Blue Channel Brightness
 */
void ledcol_setbrightness(unsigned int rb, unsigned int gb, unsigned int bb) {
    unsigned int tdata[2] = {0, 0};

    gbright_r = rb;
    gbright_g = gb;
    gbright_b = bb;

//    assert(gbright_r > MAX_CURRENT_R);  // trigger error if user overdrives LEDs
//    assert(gbright_g > MAX_CURRENT_G);
//    assert(gbright_b > MAX_CURRENT_B);

    // format control packet for transmission
    tdata[1] = LEDCOL_CMD_CONTROL | gbright_b >> 2;
    tdata[0] = gbright_b << 14 | gbright_g << 7 | gbright_r;

    // transmit to high and low column drivers
    SPI1BUF = tdata[1];
    SPI2BUF = tdata[1];
    SPI1BUF = tdata[0];
    SPI2BUF = tdata[0];
}

// TODO: Finalize display controller/column driver interface
void ledcol_display(unsigned int c0data[], unsigned int c1data[]) {
    SPI1BUF = c0data[1];
    SPI2BUF = c1data[1];
    SPI1BUF = c0data[0];
    SPI2BUF = c1data[0];
}

/** Fast disable of column drive.
 */
void ledcol_blank(void) {
    LEDCOL_PORT_C_BLANK = 1;
}

/** Fast reenable of column drive.
 */
void ledcol_unblank(void) {
    LEDCOL_PORT_C_BLANK = 0;
}

/** SPI1 Interrupt Service Handler
 *
 * Once the display string has been transmitted, latch the data and reset the
 * bus to the default state.
 */
void _ISRFAST _SPI1Interrupt(void) {
    _SPI1IF = 0;    // clear interrupt flag

    LEDCOL_PORT_C0_LAT = 1;       // latch transmitted data
    LEDCOL_PORT_C0_LAT = 0;
}

/** SPI2 Interrupt Service Handler
 *
 * Once the display string has been transmitted, latch the data and reset the
 * bus to the default state.
 */
void _ISRFAST _SPI2Interrupt(void) {
    _SPI2IF = 0;    // clear interrupt flag

    LEDCOL_PORT_C1_LAT = 1;       // latch transmitted data
    LEDCOL_PORT_C1_LAT = 0;
}

// TODO: Probably need to optimize the bitset process
inline void ledcol_bitset_r(column_data *cdata, unsigned int pos) {
    unsigned int bitp32 = (pos % 8) * 3;    // position within 32 bits
    unsigned int wordn = (bitp32 >> 4) + ((pos >> 3) << 1);
    RMBITW(cdata->data16[wordn], bitp32 % 16, 1);
}

inline void ledcol_bitset_g(column_data *cdata, unsigned int pos) {
    unsigned int bitp32 = (pos % 8) * 3 + 1;    // position within 32 bits
    unsigned int wordn = (bitp32 >> 4) + ((pos >> 3) << 1);
    RMBITW(cdata->data16[wordn], bitp32 % 16, 1);
}

inline void ledcol_bitset_b(column_data *cdata, unsigned int pos) {
    unsigned int bitp32 = (pos % 8) * 3 + 2;    // position within 32 bits
    unsigned int wordn = (bitp32 >> 4) + ((pos >> 3) << 1);
    RMBITW(cdata->data16[wordn], bitp32 % 16, 1);
}