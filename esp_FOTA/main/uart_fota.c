#include "uart_fota.h"

static const char *UART_INIT_TAG = "uart init";

void uart_init(void)
{
    uart_config_t uart0_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart0_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0,
                                         UART_RX_BUF_SIZE * 2,
                                         UART_TX_BUF_SIZE, 0, NULL, 0));

    uart_config_t uart1_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart1_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1,
                                         UART_RX_BUF_SIZE * 2,
                                         UART_TX_BUF_SIZE, 0, NULL, 0));

    ESP_LOGI(UART_INIT_TAG, "UART0 and UART1 initialized");
}
