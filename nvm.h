/* 
 * File:   nvm.h
 * Author: Matt
 *
 * Created on January 30, 2013, 5:44 PM
 */

#ifndef NVM_H
#define	NVM_H

#ifdef	__cplusplus
extern "C" {
#endif

#define NVM_ROW_SIZE        64      /**< Size of flash programming row in double bytes.*/
#define NVM_DATA_SIZE       512     /**< Total size of NVM section in double bytes.*/
#define NVM_DATA_PADDR      0xa400  /**< Address of NVM data in the program address space.*/

#define NVM_DATA_SIGLOC     511     /**< Location of validity signature in NVM block.*/
#define NVM_DATA_SIGNATURE  0x3a9d  /**< Data signature value.*/

void nvm_init(void);
int nvm_valid(void);
void nvm_program(unsigned int, unsigned int, unsigned int *);
const __psv__ unsigned int *nvm_read(unsigned int offset);

#ifdef	__cplusplus
}
#endif

#endif	/* NVM_H */

