#ifndef INC_UART_FOTA_
#define INC_UART_FOTA_

#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "esp_log.h"

/* ---- FILL THESE IN with your actual wiring ----
 * main.c uses UART_NUM_0 for the b_signal / flash_write handshake
 * with the bootloader, and UART_NUM_1 for sending firmware chunks out.
 * Adjust pins below to match your real board wiring.
 */
#define UART0_TX_PIN        1
#define UART0_RX_PIN        3

#define UART1_TX_PIN        2   /* verify against your bootloader's RX pin */
#define UART1_RX_PIN        -1  /* -1 if UART1 is TX-only in this design */

#define UART_BAUD_RATE       115200
#define UART_RX_BUF_SIZE     1024
#define UART_TX_BUF_SIZE     0   /* 0 = blocking TX, no separate TX ring buffer */

void uart_init(void);

#endif
