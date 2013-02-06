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

void USBSuspend(void);

extern unsigned char usb_device_state;

/*
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



    initCDC(); // JTR this function has been highly modified It no longer sets up CDC endpoints.
    usb_init(cdc_device_descriptor, cdc_config_descriptor, cdc_str_descs, USB_NUM_STRINGS);
    usb_start();

#if defined USB_INTERRUPTS
    EnableUsbPerifInterrupts(USB_TRN + USB_SOF + USB_UERR + USB_URST);
    EnableUsbGlobalInterrupt();
#endif

    do {
#ifndef USB_INTERRUPTS
        usb_handler();
#endif
    } while (usb_device_state < CONFIGURED_STATE);

    usb_register_sof_handler(CDCFlushOnTimeout); // Register our CDC timeout handler after device configured


    do {

        // The CDC module will call usb_handler each time a BULK CDC packet is sent or received.
        if (poll_getc_cdc(&RecvdByte)) // If there is a byte ready will return with the number of bytes available and received byte in RecvdByte
            putc_cdc(RecvdByte); //

        if (peek_getc_cdc(&RecvdByte)) { // Same as poll_getc_cdc except that byte is NOT removed from queue.
            RecvdByte = getc_cdc(); // This function will wait for a byte and return and remove it from the queue when it arrives.
            putc_cdc(RecvdByte);
        }

        if (poll_getc_cdc(&RecvdByte)) { // If there is a byte ready will return with the number of bytes available and received byte in RecvdByte
            putc_cdc(RecvdByte); //
            CDC_Flush_In_Now(); // when it has to be sent immediately and not wait for a timeout condition.
        }

    } while (1);

    return 0;
}


void __attribute__((interrupt, auto_psv)) _USB1Interrupt() {
    usb_handler();
    ClearGlobalUsbInterruptFlag();
}

void USBSuspend(void) {

}