/*
 * bootloader.c
 *
 *  Created on: Aug 30, 2025
 *      Author: abend
 *
 *
 *      Process:
 *
 *      1.) Click the Reset Button
 *      2.) Fetches reads word 0 (32-bit value) at Flash memory (it is BOOT0=0 so the Vector Table at Flash),
 *      this is the msp (Main Stack Pointer) which points to the top of the stack
 *      3.) Then reads the second word (1) which stores the reset handler address, which sets clocks, copys .data from flash
 *      to SRAM, zero .bss, etc., then it runs the firmware img we uploaded or have at 0x8008000
 *
 */

#include <string.h>

#include "bootloader.h"
#include "stm32f4xx_hal.h"

#include "usart.h"

extern UART_HandleTypeDef huart2;

static int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);
    return ch;
}

/*
 * Fetches the first value in the Vector Table, which is the Main Stack Pointer.
 * Returns 1 if the MSP is the range of the correct SRAM region
 */
int IMG_valid(uint32_t addr){
	uint32_t msp = *((volatile uint32_t *) addr);
	printf("0x%08lx\n\r", msp);
	return (msp >= START_SRAM) && (msp <= END_SRAM);
}

/*
 * Creates a function pointer to point to address of the reset handler.
 * Then runs the Reset Handler, which switches to the image stored in section 1 of RAM
 */
__attribute__((noreturn)) static inline void jump_to_reset(uint32_t rh_addr){


	void (*reset_fn)(void) = (void(*)(void)) rh_addr;
	reset_fn();
}

/*
 * Store the dereferenced type-casted macro where both the MSP and Reset Handler are stored in the Vector Table.
 * In order to switch, you have to disable any interrupts, SysTick, and peripherals
 */
void jmp_to_IMG(uint32_t addr){


	uint32_t msp_addr  = *(uint32_t *) (addr + 0x00);
	uint32_t reset_addr = *(uint32_t *) (addr + 0x04);

	__disable_irq();

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL  = 0;

	uint32_t blocks = ((SCnSCB->ICTR & 0xF) + 1); // number of ICER/ICPR regs

	for (uint32_t i = 0; i < blocks; ++i) {
		NVIC->ICER[i] = 0xFFFFFFFF;
	    NVIC->ICPR[i] = 0xFFFFFFFF;
	}

	SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk;


	HAL_DeInit();
	HAL_RCC_DeInit();


	SCB->VTOR = (uint32_t)addr;
	__DSB();
	__ISB();

	__HAL_FLASH_DATA_CACHE_DISABLE();
	__HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
	__HAL_FLASH_DATA_CACHE_RESET();
	__HAL_FLASH_INSTRUCTION_CACHE_RESET();
	__HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
	__HAL_FLASH_DATA_CACHE_ENABLE();

	__set_CONTROL(0);
	__ISB();
	__set_MSP(msp_addr);

	__set_BASEPRI(0);
	__set_FAULTMASK(0);
	__enable_irq();


	jump_to_reset(reset_addr);
}

/*
 *	This function clears the sector 2 memory region and creates a new BCB struct.
 *	Then copies this struct into first couple words of the memory region.
 *
 *	Function should only really be used if there are errors with the BCB or accidently overwrite the BCB struct.
 *	Makefile that uploads that uses the ARM toolset
 */


void bcb_reset(void){

	bcb_t bcb = {0};
	bcb.active_slot = SLOT_A;
	int num_word = (sizeof(bcb_t) + 3)/4;

	HAL_FLASH_Unlock();

	FLASH_Erase_Sector(FLASH_SECTOR_2, FLASH_VOLTAGE_RANGE_3);
		for(int i = 0; i < num_word; i ++){

			HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BCB_SEC + (i * 4) , *(((uint32_t *)&bcb) + i));
			printf("value: %d", *(((uint32_t *)&bcb) + i));
		}
	HAL_FLASH_Lock();
}

void bcb_slotswitch(uint8_t active_slot){

	bcb_t bcb = {0};
	bcb.active_slot = (active_slot) ? SLOT_A : SLOT_B;
	printf("inside bcb_slotswitch: %d\n\r", bcb.active_slot);
	int num_word = (sizeof(bcb_t) + 3)/4;

	HAL_FLASH_Unlock();

	FLASH_Erase_Sector(FLASH_SECTOR_2, FLASH_VOLTAGE_RANGE_3);


	for(int i = 0; i < num_word; i ++){
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BCB_SEC + (i * 4) , *(((uint32_t *)&bcb) + i));

	}
	printf("write sucessful in bcb\n\r");
	HAL_FLASH_Lock();
}

FlashStatus write_to_flash(fw_chunk_t* fw_chunk, uint32_t address){
	uint16_t length = fw_chunk->length;

	if(length == 0) return FLASH_ERR;

	const uint8_t *src = fw_chunk->fw_img;

	uint32_t n = length / 4;
	uint32_t offset = n * 4;
	uint32_t tail = length % 4;

	HAL_FLASH_Unlock();
	for(uint32_t i = 0 ; i < n; i++){
		uint32_t val;
		memcpy(&val, &src[i*4], sizeof(uint32_t));
		uint32_t dst = address + (i * 4);

		if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst , val) != HAL_OK){
			HAL_FLASH_Lock();
			return FLASH_ERR;
		}
	}

	if(tail){
		uint32_t padding = 0xffffffff;
		memcpy(&padding, &src[offset], tail);
		if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + offset, padding) != HAL_OK){
			HAL_FLASH_Lock();
			return FLASH_ERR;
		}
	}

	HAL_FLASH_Lock();

	return FLASH_OK;
}

uint32_t compute_crc(const uint8_t *data, uint32_t len){
    uint32_t crc = 0xffffffffU;
    while (len--) {
        crc ^= (uint32_t)(*data++);
        for (int i = 0; i < 8; i++) {
            uint32_t mask = -(crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return crc ^ 0xffffffffU;
}
