/*
 * bootloader.h
 *
 *  Created on: Aug 30, 2025
 *      Author: abend
 */
#include <stdint.h>

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_



#define IMG_SEC 	0x08008000UL

#define START_SRAM	0x20000000UL
#define END_SRAM	0x20018000UL

int IMG_valid(void);
static inline void jump_to_reset(uint32_t rh_addr);
void jmp_to_IMG();

#endif /* INC_BOOTLOADER_H_ */
