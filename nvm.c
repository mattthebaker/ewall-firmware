#include <xc.h>

#include "nvm.h"

__psv__ __attribute__((space(psv),address(NVM_DATA_PADDR)))
        const unsigned int nvm_data[NVM_DATA_SIZE];

unsigned int nvm_valid = 0;

void nvm_init(void) {
    if (nvm_data[NVM_DATA_SIGLOC] == NVM_DATA_SIGNATURE)
        nvm_valid = 1;
}

const __psv__ unsigned int *nvm_read(unsigned int offset) {
    if (nvm_valid)
        return nvm_data + offset;

    return (0);
}

void nvm_program(unsigned int len, unsigned int offset, unsigned int * data) {
    unsigned int i = 0;
    unsigned int tmpdata[NVM_DATA_SIZE];

    while (i < offset) {             // copy data before offset to temp buffer
        tmpdata[i] = nvm_data[i];
        i++;
    }

    while (i < offset + len)       // position new data
        tmpdata[i++] = *(data++);
                           
    while (i < NVM_DATA_SIZE) {      // copy data after offset+len to buffer
        tmpdata[i] = nvm_data[i];
        i++;
    }
    
    tmpdata[NVM_DATA_SIGLOC] = NVM_DATA_SIGNATURE;

    // erase memory section
    _NVMOP = 2;
    _ERASE = 1;
    _WREN = 1;
    TBLPAG = 0;
    __builtin_tblwtl(NVM_DATA_PADDR & 0xFFFF, 0);
    __builtin_disi(10);
    __builtin_write_NVM();

    _ERASE = 0;
    _NVMOP = 1;

    for (i = 0; i < NVM_DATA_SIZE; i++) {
        __builtin_tblwtl(NVM_DATA_PADDR + i * 2, tmpdata[i]);    // write row latch word by word
        if (i % NVM_ROW_SIZE == NVM_ROW_SIZE - 1) {     // program every 64 words
            __builtin_disi(10);
            __builtin_write_NVM();
        }
    }

    _WREN = 0;

}