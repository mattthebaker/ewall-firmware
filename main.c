/* 
 * File:   main.c
 * Author: Matt
 *
 * Created on January 27, 2013, 2:55 PM
 */

#include <xc.h>

//#include <stdio.h>
//#include <stdlib.h>

#include "dp_usb/usb_stack_globals.h"
#include "descriptors.h"

#include "board.h"
#include "ledrow.h"
#include "ledcol.h"
#include "touch.h"
#include "display.h"
#include "touchmap.h"
#include "nvm.h"

#define CMD_SHOW_ROUTE          0x01
#define CMD_HIDE_ROUTE          0x02
#define CMD_SET_BRIGHTNESS      0x03
#define CMD_GET_BRIGHTNESS      0x04
#define CMD_GET_HOLD            0x05
#define CMD_GET_CAPABILITIES    0x06
#define CMD_GET_TOUCHMAP        0x07
#define CMD_RETRAIN_TOUCHMAP    0x08
#define CMD_RAW_TOUCH_MODE      0x09
#define CMD_RESET               0x0a

#define CMD_SEND_BRIGHTNESS     0x04
#define CMD_SEND_HOLD           0x05
#define CMD_SEND_CAPABILITIES   0x06
#define CMD_SEND_TOUCHMAP       0x07
#define CMD_SEND_RAWTOUCH       0x09

#define CMD_BUFFER_SIZE         120
#define TOUCHTX_BUFFER_SIZE     45

extern unsigned char usb_device_state;

static unsigned int atoi(char *);
static unsigned int atoi_next(char *, unsigned char *);
static void putuchar_cdc(unsigned char, unsigned char);
static void command_process(void);
static void rawtouch_cb(unsigned int);
static void rawrelease_cb(unsigned int);
static void gethold_cb(unsigned int);
static void rawtouch_send(unsigned char, unsigned int);

void USBSuspend(void);

static unsigned char cmd_bufferfree = 1;    /**< Buffer ready flag. */
static unsigned char cmd_bufferindex = 0;   /**< Present position in buffer. */
static unsigned char cmd_processflag = 0;   /**< Flag that a command is ready for processing. */
static char cmd_buffer[CMD_BUFFER_SIZE];    /**< Command buffer. */

static unsigned char touchtx_count = 0;     /**< Bytes in tx buffer. */
static char touchtx_buffer[TOUCHTX_BUFFER_SIZE];    /**< Transmit buffer. */
static unsigned int touchtx_miss = 0;   /**< Events missed due to full buffer. */

/** Main function. 
 * 
 */
int main(int argc, char** argv) {
    unsigned char RecvdByte;

    board_init();
    ledrow_init();
    ledcol_init();
    touch_init();
    display_init();
    nvm_init();
    touchmap_init();

    ledcol_enable();
    ledcol_clear();
    display_enable();

//    touch_enable();
//    while (1) touch_process();

    initCDC();
    usb_init(cdc_device_descriptor, cdc_config_descriptor, cdc_str_descs, USB_NUM_STRINGS);
    usb_start();

    EnableUsbPerifInterrupts(USB_TRN + USB_SOF + USB_UERR + USB_URST);
    EnableUsbGlobalInterrupt();

/*    do {
        usb_handler();
    } */while (usb_device_state < CONFIGURED_STATE);

    usb_register_sof_handler(CDCFlushOnTimeout); // Register our CDC timeout handler after device configured

    while (1) {
//        usb_handler();
        display_process();
        touch_process();
//        touchmap_process();

        if (cmd_processflag)
            command_process();

        if (touchtx_count) {
            char lbuf[TOUCHTX_BUFFER_SIZE];
            int i, count;

            __builtin_disi(0x3fff);
            for (i = 0; i < touchtx_count; i++)
                lbuf[i] = touchtx_buffer[i];

            count = touchtx_count;
            touchtx_count = 0;
            __builtin_disi(0);

            for (i = 0; i < count; i++)
                putc_cdc(lbuf[i]);

            CDC_Flush_In_Now();
        }

        if (cmd_bufferfree)
            while (poll_getc_cdc(&RecvdByte)) {
                if (RecvdByte == '\n') {
                    cmd_buffer[cmd_bufferindex] = 0;
                    cmd_bufferfree = 0;
                    cmd_bufferindex = 0;
                    cmd_processflag = 1;
                }
                else {
                    if (cmd_bufferindex == CMD_BUFFER_SIZE)
                        cmd_bufferindex = 0;
                    if (RecvdByte != '\r')         
                        cmd_buffer[cmd_bufferindex++] = RecvdByte;
                }
                // TODO: Add some form of error if the command overflows the buffer.
            }
    }

    return 0;
}

/** USB interrupt handler. */
void __attribute__((interrupt, auto_psv)) _USB1Interrupt() {
    usb_handler();
    ClearGlobalUsbInterruptFlag();
}

/** USB suspend event. */
void USBSuspend(void) {

}

/** Convert string to integer. */
static unsigned int atoi(char *str) {
    unsigned int retval = 0;

    while (*str >= '0' && *str <= '9')
        retval = (retval * 10) + (*(str++) - '0');

    return retval;
}

/** Convert string to integer, and return characters processed. */
static unsigned int atoi_next(char *str, unsigned char *target) {
    unsigned int diff;
    char *pstr = str;
    *target = 0;

    while (*pstr >= '0' && *pstr <= '9')
        *target = (*target * 10) + (*(pstr++) - '0');

    diff = pstr - str;
    if (diff > 0)
        diff++;

    return diff;
}

/** Convert a uchar to string and transmit it. */
static void putuchar_cdc(unsigned char num, unsigned char trail) {
    unsigned char tmpc;

    if ((tmpc = num / 100))
        putc_cdc(tmpc + '0');
    if ((tmpc = (num / 10) % 10))
        putc_cdc(tmpc + '0');
    putc_cdc(num % 10 + '0');
    putc_cdc(trail);
}

/** Create data packet for a touch/release event. */
static void rawtouch_send(unsigned char touchrelease, unsigned int channel) {
    if (TOUCHTX_BUFFER_SIZE - touchtx_count < 10) {
        touchtx_miss++;
        return;
    }
    
    touchtx_buffer[touchtx_count++] = CMD_SEND_RAWTOUCH / 10 + '0';
    touchtx_buffer[touchtx_count++] = CMD_SEND_RAWTOUCH % 10 + '0';
    touchtx_buffer[touchtx_count++] = ' ';
    touchtx_buffer[touchtx_count++] = touchrelease;
    touchtx_buffer[touchtx_count++] = ' ';
    touchtx_buffer[touchtx_count++] = (channel & 0x1F) / 10 + '0';
    touchtx_buffer[touchtx_count++] = (channel & 0x1F) % 10 + '0';
    touchtx_buffer[touchtx_count++] = ' ';
    touchtx_buffer[touchtx_count++] = ((channel & 0xE0) >> 5) + '0';
    touchtx_buffer[touchtx_count++] = '\n';
}

/** Touch press event callback. */
static void rawtouch_cb(unsigned int channel) {
    rawtouch_send('1', channel);
}

/** Touch release event callback. */
static void rawrelease_cb(unsigned int channel) {
    rawtouch_send('0', channel);
}

/** GetHold functionality callback. */
static void gethold_cb(unsigned int hold) {
    if (TOUCHTX_BUFFER_SIZE - touchtx_count < 7) {
        touchtx_miss++;
        return;
    }

    touchtx_buffer[touchtx_count++] = CMD_SEND_HOLD / 10 + '0';
    touchtx_buffer[touchtx_count++] = CMD_SEND_HOLD % 10 + '0';
    touchtx_buffer[touchtx_count++] = ' ';
    touchtx_buffer[touchtx_count++] = hold / 100 + '0';
    touchtx_buffer[touchtx_count++] = hold / 10 % 10 + '0';
    touchtx_buffer[touchtx_count++] = hold % 10 + '0';
    touchtx_buffer[touchtx_count++] = '\n';
}

/** Process command from serial interface. */
void command_process(void) {
    const __psv__ unsigned char *nvmdata;
    unsigned char cmd;
    unsigned char r, g, b;
    char *cpos = cmd_buffer;
    route newroute;
    int i, j;

    cpos += atoi_next(cpos, &cmd);

    switch (cmd) {
        case CMD_SHOW_ROUTE:    // send route to display controller.
            i = 0;

            cpos += atoi_next(cpos, &newroute.id);
            cpos += atoi_next(cpos, &newroute.len);
            cpos += atoi_next(cpos, &newroute.r);
            cpos += atoi_next(cpos, &newroute.g);
            cpos += atoi_next(cpos, &newroute.b);
            cpos += atoi_next(cpos, &newroute.heartbeat);
            
            while (*cpos != '\0' && i < DISPLAY_ROUTE_LEN)
                cpos += atoi_next(cpos, &newroute.holds[i++]);

            display_showroute(&newroute);
            break;

        case CMD_HIDE_ROUTE:    // hide route in display controller.
            display_hideroute(atoi(cpos));
            break;

        case CMD_SET_BRIGHTNESS:    // set brightness of LED drivers.
            cpos += atoi_next(cpos, &r);
            cpos += atoi_next(cpos, &g);
            cpos += atoi_next(cpos, &b);

            ledcol_setbrightness(r, g, b);
            break;

        case CMD_GET_BRIGHTNESS:    // get present brightness
            putc_cdc(CMD_SEND_BRIGHTNESS / 10 + '0');
            putc_cdc(CMD_SEND_BRIGHTNESS % 10 + '0');
            putc_cdc(' ');

            ledcol_getbrightness(&r, &g, &b);

            putuchar_cdc(r, ' ');
            putuchar_cdc(g, ' ');
            putuchar_cdc(b, '\n');
            CDC_Flush_In_Now();

            break;

        case CMD_GET_HOLD:  // solicit hold input from the user
            touchmap_gethold(gethold_cb);
            break;

        case CMD_GET_CAPABILITIES:  // send information on capabilities
            putc_cdc(CMD_SEND_CAPABILITIES / 10 + '0');
            putc_cdc(CMD_SEND_CAPABILITIES % 10 + '0');
            putc_cdc(' ');

            putuchar_cdc(DISPLAY_MAX_ROUTES, ' ');
            putuchar_cdc(DISPLAY_ROUTE_LEN, '\n');
            CDC_Flush_In_Now();

            break;

        case CMD_GET_TOUCHMAP:  // send touch->hold map
            nvmdata = (const __psv__ unsigned char *)nvm_read(0);

            putc_cdc(CMD_SEND_TOUCHMAP / 10 + '0');
            putc_cdc(CMD_SEND_TOUCHMAP % 10 + '0');
            putc_cdc(' ');

            for (i = 0; i < DISPLAY_ROWS; i++) {
                for (j = 0; j < DISPLAY_COLS; j++)
                    putuchar_cdc(nvmdata[i * DISPLAY_ROWS + j], ' ');
                if (i == DISPLAY_ROWS - 1)
                    putc_cdc('\n');
                CDC_Flush_In_Now();
            }

            break;
            
        case CMD_RETRAIN_TOUCHMAP:  // train the touchmap
            touchmap_train();
            break;

        case CMD_RAW_TOUCH_MODE:    // enter raw touch mode to relay events over serial
            touch_setcallbacks(rawtouch_cb, rawrelease_cb);
            touch_enable();
            break;

        case CMD_RESET:             // reset the board
            asm("RESET");
            break;
            
        default:
            break;
    }

    cmd_bufferfree = 1;
    cmd_processflag = 0;
}