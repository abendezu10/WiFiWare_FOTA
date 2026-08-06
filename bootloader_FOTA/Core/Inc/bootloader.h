/*
 * bootloader.h
 *
 *  Created on: Aug 30, 2025
 *      Author: abend
 */
#include <stdint.h>

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

/* ============================================================
 * Memory map
 * ------------------------------------------------------------
 * PLACEHOLDER VALUES — verify these against your actual linker
 * script and the FLASH_SECTOR_x erase calls in bootloader.c.
 *
 * Inferred from bootloader.c's erase calls:
 *   bcb_init/bcb_slotswitch  -> FLASH_SECTOR_2
 *   Slot A erase             -> FLASH_SECTOR_3, 4, 5
 *   Slot B erase             -> FLASH_SECTOR_6, 7
 *
 * On STM32F401RE (512KB flash), the sector map is:
 *   Sector 0: 0x08000000 - 0x08003FFF (16KB)
 *   Sector 1: 0x08004000 - 0x08007FFF (16KB)
 *   Sector 2: 0x08008000 - 0x0800BFFF (16KB)
 *   Sector 3: 0x0800C000 - 0x0800FFFF (16KB)
 *   Sector 4: 0x08010000 - 0x0801FFFF (64KB)
 *   Sector 5: 0x08020000 - 0x0803FFFF (128KB)
 *   Sector 6: 0x08040000 - 0x0805FFFF (128KB)
 *   Sector 7: 0x08060000 - 0x0807FFFF (128KB)
 *
 * NOTE: this means IMG_SEC (0x08008000, sector 2) and BCB_SEC
 * below currently point at the SAME address. That's likely a
 * leftover from before slots existed — IMG_SEC may be dead/unused
 * now that SLOT_A_SEC/SLOT_B_SEC exist. Double check whether
 * IMG_SEC is still referenced anywhere before relying on it.
 * ============================================================ */

#define IMG_SEC 	0x08008000UL

#define START_SRAM	0x20000000UL
#define END_SRAM	0x20018000UL

#define BCB_SEC     0x08008000UL   /* Sector 2 - VERIFY vs IMG_SEC collision above */

#define SLOT_A_SEC  0x0800C000UL   /* Start of sector 3 */
#define SLOT_B_SEC  0x08040000UL   /* Start of sector 6 */

#define FW_IMG_SIZE 512            /* Matches ESP8266's FW_BUFFER_SIZE */

/* ============================================================
 * BCB (Boot Control Block)
 * ============================================================ */
#define SLOT_A  1
#define SLOT_B  0

typedef struct {
    uint8_t active_slot;   /* SLOT_A or SLOT_B */
} bcb_t;

/* ============================================================
 * Firmware chunk (received over UART from ESP8266)
 * ------------------------------------------------------------
 * WARNING: This must match the ESP8266's fw_chunk_t byte-for-byte,
 * since UART is raw bytes with no serialization layer. The ESP8266
 * side (http_fota.h) currently does NOT have a `crc` field and is
 * NOT `packed` the same way, if at all. Confirm both sides agree
 * before trusting the CRC check in main.c — otherwise fields will
 * misalign across the wire.
 * ============================================================ */
typedef struct __attribute__((packed)) {
    uint16_t length;
    uint8_t  last_chunk;
    uint32_t crc;
    uint8_t  fw_img[FW_IMG_SIZE];
} fw_chunk_t;

/* ============================================================
 * Flash write result
 * ============================================================ */
typedef enum {
    FLASH_OK  = 0,
    FLASH_ERR = 1
} FlashStatus;

/* ============================================================
 * Function prototypes
 * ============================================================ */
int  IMG_valid(uint32_t addr);
static inline void jump_to_reset(uint32_t rh_addr);
void jmp_to_IMG(uint32_t addr);

void bcb_init(void);
void bcb_slotswitch(uint8_t active_slot);

FlashStatus write_to_flash(fw_chunk_t *fw_chunk, uint32_t address);

#endif /* INC_BOOTLOADER_H_ */
