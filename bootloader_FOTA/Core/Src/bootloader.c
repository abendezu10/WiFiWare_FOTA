/*
 * bootloader.c
 *
 *  Created on: Aug 30, 2025
 *      Author: abend
 */

#include "bootloader.h"
#include "stm32f4xx_hal.h"


/*
 * Fetches the first value in the Vector Table, which is the Main Stack Pointer.
 * Returns 1 if the MSP is the range of the correct SRAM region
 */
int IMG_valid(void){
	uint32_t msp = *((volatile uint32_t *) IMG_SEC);
	return (msp >= START_SRAM) && (msp <= END_SRAM);
}

/*
 * Creates a function pointer to point to address of the reset handler.
 * Then runs the Reset Handler, which switches to the image stored in section 1 of RAM
 */
static inline void jump_to_reset(uint32_t rh_addr){
	void (*reset_fn)(void) = (void(*)(void)) rh_addr;
	reset_fn();
}


/*
 * Store the dereferenced type-casted macro where both the MSP and Reset Handler are stored in the Vector Table.
 * In order to switch, you have to disable any interrupts, SysTick, and peripherals
 */
void jmp_to_IMG(){


	uint32_t msp_addr  = *(uint32_t *) (IMG_SEC + 0x00);
	uint32_t reset_addr = *(uint32_t *) (IMG_SEC + 0x04);

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


	SCB->VTOR = (uint32_t)IMG_SEC;
	__DSB();
	__ISB();


	__set_CONTROL(0);
	__ISB();
	__set_MSP(msp_addr);


	jump_to_reset(reset_addr);
}







