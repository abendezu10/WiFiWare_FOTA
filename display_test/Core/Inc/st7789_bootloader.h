/*
 * st7789_bootloader.h
 *
 *  Created on: Aug 6, 2026
 *      Author: Alexander Bendezu
 */

#ifndef INC_ST7789_BOOTLOADER_H_
#define INC_ST7789_BOOTLOADER_H_


/* Call once when entering bootloader */
void UI_Bootloader_ShowStart(void);

/* Call periodically during download (0–100) */
void UI_Bootloader_UpdateProgress(uint8_t percent);

/* Optional: call to update the "..." animation if you don't have real % */
void UI_Bootloader_TickDots(void);

/* Call once when update completes successfully */
void UI_Bootloader_ShowDone(void);

void UI_Bootloader_ShowFailed(void);

#endif /* INC_ST7789_BOOTLOADER_H_ */
