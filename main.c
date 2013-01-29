/* 
 * File:   main.c
 * Author: Matt
 *
 * Created on January 27, 2013, 2:55 PM
 */

#include <xc.h>

//#include <stdio.h>
//#include <stdlib.h>

#include "board.h"
#include "ledrow.h"
#include "ledcol.h"

/*
 * 
 */
int main(int argc, char** argv) {
    board_init();
    ledrow_init();
    ledcol_init();

    return 0;
}

