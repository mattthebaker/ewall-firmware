#include <xc.h>
#include <stddef.h>

#include "nvm.h"

/** NVM data block.  The data is accessible through the PSV space
 * without directly issuing read commands.  This both provides a pointer to
 * to the NVM data, and reserves the memory locations within the program space. */
__psv__ __attribute__((space(psv),address(NVM_DATA_PADDR)))
        static const unsigned int nvm_data[NVM_DATA_SIZE];  

/** Flag indicating whether the NVM segment is valid. */
static int nvmvalid;

/** Initialize NVM module.
 */
void nvm_init(void) {
    nvmvalid = 0;
    if (nvm_data[NVM_DATA_SIGLOC] == NVM_DATA_SIGNATURE)
        nvmvalid = 1;
}

/** Check validity of NVM data.
 * @return Data valid flag.
 */
int nvm_valid(void) {
    return nvmvalid;
}

/** Get NVM data at the specified offset.
 * With the PSV capability of the PIC24F, the NVM data is directly read
 * from program memory when the pointer is accessed.
 * @param offset Location in NVM block to return.
 * @return pointer to the data or NULL if the data is invalid.
 */
const __psv__ unsigned int *nvm_read(unsigned int offset) {
    if (nvmvalid)
        return nvm_data + offset;

    return NULL;
}

/** Program the NVM data.
 * Updates the NVM data block with the values passed in the arguments.
 * @param len Length of the data block in double bytes.
 * @param offset Offset within the NVM block to store the data.
 * @param data Pointer to the data to be written.
 */
void nvm_program(unsigned int len, unsigned int offset, unsigned int * data) {
    unsigned int i = 0;
    unsigned int tmpdata[NVM_DATA_SIZE];

    while (i < offset) {            // copy data before offset to temp buffer
        tmpdata[i] = nvm_data[i];
        i++;
    }

    while (i < offset + len)        // copy new data into position
        tmpdata[i++] = *(data++);
                           
    while (i < NVM_DATA_SIZE) {     // copy data after offset+len to buffer
        tmpdata[i] = nvm_data[i];
        i++;
    }
    
    tmpdata[NVM_DATA_SIGLOC] = NVM_DATA_SIGNATURE;  // set signature

    // erase memory section
    _NVMOP = 2;
    _ERASE = 1;
    _WREN = 1;
    TBLPAG = 0;
    __builtin_tblwtl(NVM_DATA_PADDR & 0xFFFF, 0);   // set pointer to flash block
    __builtin_disi(10);
    __builtin_write_NVM();                          // issue erase command

    _ERASE = 0;
    _NVMOP = 1;

    for (i = 0; i < NVM_DATA_SIZE; i++) {
        __builtin_tblwtl(NVM_DATA_PADDR + i * 2, tmpdata[i]);    // write row latch word by word
        if (i % NVM_ROW_SIZE == NVM_ROW_SIZE - 1) {     // program every 64 words
            __builtin_disi(10);
            __builtin_write_NVM();
        }
    }

    _WREN = 0;  // disable flash commands

    nvmvalid = 0;   // verify data validity
    if (nvm_data[NVM_DATA_SIGLOC] == NVM_DATA_SIGNATURE)
        nvmvalid = 1;
}