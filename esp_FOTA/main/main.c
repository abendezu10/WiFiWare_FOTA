#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "http_fota.h"
#include "wifi_fota.h"
#include "uart_fota.h"

#include "mbedtls/aes.h"



#include "rom/crc.h"

#include <netdb.h>
#include <sys/socket.h>
#include "lwip/sockets.h"

#define MAGIC_LEN   4
#define FW_SIZE_LEN 4
#define IV_LEN   16
#define PACKAGE_LEN  (MAGIC_LEN + FW_SIZE_LEN + IV_LEN)

static const char *UART_TAG = "uart receiver";
static const char *HTTP_TAG = "http client";
static char REQUEST[256]= "GET " WEB_PATH " HTTP/1.1\r\n"
         "Host: " WEB_SERVER "\r\n"
         "User-Agent: esp-idf/1.0 esp32\r\n"
         "\r\n";


static QueueHandle_t qBuffer;

static uint8_t package[PACKAGE_LEN];

static char magic[5];
static uint32_t fw_size;
static uint8_t iv[16];

static volatile uint8_t task_send_done = 0;

static mbedtls_aes_context aes_ctx;
static uint8_t ctr_nonce_counter[16];   // IV / counter state
static uint8_t stream_block[16];        // internal keystream buffer
static size_t nc_off = 0;               // offset in stream_block
static bool aes_ready = false;          // set true once header parsed


const uint8_t AES_KEY[32] = {
                                    0xae, 0xfd, 0xc3, 0x22,
                                    0xe1, 0x27, 0xa3, 0x20,
                                    0x95, 0x30, 0x6e, 0xdb,
                                    0x11, 0xec, 0x7b, 0x1a,
                                    0x19, 0xd6, 0xb7, 0xae,
                                    0x58, 0xb6, 0x84, 0x0c,
                                    0x6a, 0xdf, 0x07, 0x60,
                                    0x09, 0xf7, 0x4d, 0x24
                                };



void taskRecieveFirmware(void *pvParameter);
void taskSendFirmware(void *pvParameter);

//int aes_gcm_decrypt_firmware(const uint8_t *cipher_fw, uint32_t fw_size, uint8_t *plain_fw)
/*
    Next things to do:
    1.) CRC validation
    2.) create pytohn server that runs server and encrypts
      a.) then task in esp decrypts and sends it nroamlly
    3.) Cmake file to build everything and change linkerscript to match bcb

    4.) Get display to work and record video for portfolio (last thing)

    FINISHED


*/


void app_main(void)
{

    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();
    uart_init();

    qBuffer = xQueueCreate(FW_QUEUE_SIZE, sizeof(fw_chunk_t));
    configASSERT(qBuffer);


    xTaskCreate(&taskRecieveFirmware, "Firmware Recieving Task", 4096,NULL, 1, NULL);

    xTaskCreate(&taskSendFirmware, "Firmware Sending Task", 4096, NULL, 2, NULL);


}

void taskRecieveFirmware(void *pvParameter){

    uart_flush_input(UART_NUM_0);

    // Using TCP socket, for reliable transport between ESP and HTTP Server(PC)
    const struct addrinfo hints = {
        .ai_family = AF_INET, // IPv4 addresses
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res;
    struct in_addr *addr;
    int sock, received_http;

    for(;;){
        uint8_t b_signal = 0; // Bootloader sends UART signal to ESP that it wants to update firmware
        int len = uart_read_bytes(UART_NUM_0, &b_signal, 1, pdMS_TO_TICKS(5000));
        if(len && b_signal){
            ESP_LOGI(UART_TAG, "b_signal changed from 0 to 1");

            int error = getaddrinfo(WEB_SERVER, "8000", &hints, &res);
            if(error != 0 || res == NULL){
                ESP_LOGE(HTTP_TAG, "DNS lookup has failed err %d res %p", error, res);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            }
            addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
            ESP_LOGI(HTTP_TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));


                // socket allocation
            sock = socket(res->ai_family, res->ai_socktype, 0);
            if(sock < 0 ){
                ESP_LOGE(HTTP_TAG, "Failed to allocate socket");
                freeaddrinfo(res);
                vTaskDelay(portTICK_PERIOD_MS);
                continue;
            }

            ESP_LOGI(HTTP_TAG, "We have allocated the socket");

            if(connect(sock, res->ai_addr, res->ai_addrlen) != 0){
                ESP_LOGE(HTTP_TAG, "socket connecting has failed");
                close(sock);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }

            ESP_LOGI(HTTP_TAG, "socket send was successful");
            freeaddrinfo(res);

            if(write(sock, REQUEST, strlen(REQUEST)) < 0){
                ESP_LOGE(HTTP_TAG, "socket sending has failed");
                close(sock);
                continue;
            }

            ESP_LOGI(HTTP_TAG, "socket sending was successful");

            struct timeval receiving_timeout;
            receiving_timeout.tv_sec = 5;
            receiving_timeout.tv_usec = 0;

            if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0){
                ESP_LOGE(HTTP_TAG, "failed to set socket receiving timeout");
                close(sock);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGI(HTTP_TAG, "set socket receiving timeout success");



            int eofheader = 0, eofpackage = 0;
            int track = 0;
            int prev_fw_len = 0;
            uint32_t total_fw_bytes = 0;

            fw_chunk_t fw_chunk = {.fw_length = 0, .last_chunk = 0};
            uint8_t http_buffer[FW_BUFFER_SIZE];



            do{
                memset(http_buffer, 0, sizeof(http_buffer));

                received_http = read(sock, http_buffer, sizeof(http_buffer));
                ESP_LOGI(HTTP_TAG, "Received HTTP: %d [%d]", received_http, track);

                // Sending out Firmware Image chunk
                if (eofpackage && eofheader) {

                    if (fw_chunk.fw_length < FW_BUFFER_SIZE) {

                        size_t space_left = FW_BUFFER_SIZE - fw_chunk.fw_length;
                        size_t src_len    = (size_t)received_http;

                        // don't read past the end of the firmware image
                        size_t remaining_fw = (fw_size > total_fw_bytes)
                                            ? (fw_size - total_fw_bytes)
                                            : 0;

                        size_t to_add = space_left;
                        if (to_add > src_len)      to_add = src_len;
                        if (to_add > remaining_fw) to_add = remaining_fw;

                        if (to_add == 0) {

                            if (fw_chunk.fw_length > 0 && total_fw_bytes == fw_size) {
                                fw_chunk.last_chunk = 1;
                                xQueueSend(qBuffer, &fw_chunk, portMAX_DELAY);
                                ESP_LOGI(HTTP_TAG, "HOWDY");
                                ESP_LOGI(HTTP_TAG, "The bytes received are: %d and now being sent to queue [%d]", received_http, track);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[0]);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[1]);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[2]);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[3]);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[4]);
                                track++;
                            }
                            break; // done
                        }

                        memcpy(&fw_chunk.fw_img[fw_chunk.fw_length], &http_buffer[0], to_add);
                        fw_chunk.fw_length   += to_add;
                        total_fw_bytes       += to_add;

                        size_t remaining = src_len - to_add;
                        size_t offset    = to_add;

                        // if we've just reached the end of the firmware, mark last chunk
                        if (total_fw_bytes == fw_size) {
                            fw_chunk.last_chunk = 1;
                        }

                        if (fw_chunk.fw_length == FW_BUFFER_SIZE || fw_chunk.last_chunk) {
                            fw_chunk_t out = fw_chunk;

                            if (xQueueSend(qBuffer, &out, portMAX_DELAY) != pdPASS)
                                ESP_LOGI(HTTP_TAG, "Sending firmware buffer to the Send Task has failed!");
                            else{
                                ESP_LOGI(HTTP_TAG, "The bytes received are: %d and now being sent to queue [%d]", received_http, track);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[0]);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[1]);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[2]);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[3]);
                                ESP_LOGI(HTTP_TAG, "%02X\n\r", fw_chunk.fw_img[4]);
                                track++;

                            }
                            // reset chunk state
                            fw_chunk.last_chunk = 0;
                            fw_chunk.fw_length  = 0;
                            memset(fw_chunk.fw_img, 0, FW_BUFFER_SIZE);

                            // if we still have bytes from this HTTP read AND we haven’t finished firmware,
                            // start filling the next chunk
                            if (remaining > 0 && total_fw_bytes < fw_size) {
                                size_t second_copy = remaining;
                                size_t remaining_fw2 = fw_size - total_fw_bytes;
                                if (second_copy > FW_BUFFER_SIZE)  second_copy = FW_BUFFER_SIZE;
                                if (second_copy > remaining_fw2)   second_copy = remaining_fw2;

                                memcpy(fw_chunk.fw_img, &http_buffer[offset], second_copy);
                                fw_chunk.fw_length = second_copy;
                                total_fw_bytes    += second_copy;

                                if (total_fw_bytes == fw_size) {
                                    fw_chunk.last_chunk = 1;
                                }
                            }
                        }
                    }

                    continue;
                }

                ESP_LOGI(HTTP_TAG, "First bytes of raw cipher:");
                for (int i = 0; i < 16; ++i) {
                    ESP_LOGI(HTTP_TAG, "%02X", fw_chunk.fw_img[i]);
                }



                // Packet Parsing
                if (!eofpackage && eofheader) {

                    if (prev_fw_len == 0) {

                        memcpy(&package[0], &http_buffer[0], PACKAGE_LEN);
                        memcpy(&fw_chunk.fw_img[0], &http_buffer[PACKAGE_LEN], received_http - PACKAGE_LEN);
                        fw_chunk.fw_length = received_http - PACKAGE_LEN;
                        // 472 bytes / 512

                    } else if (prev_fw_len > 0) {
                        // If that assumption is wrong with your server pattern, this logic needs rework.
                        memcpy(&package[0], &fw_chunk.fw_img[0], PACKAGE_LEN);
                        memmove(&fw_chunk.fw_img[0], &fw_chunk.fw_img[PACKAGE_LEN], prev_fw_len - PACKAGE_LEN);
                        fw_chunk.fw_length = prev_fw_len - PACKAGE_LEN;
                        total_fw_bytes = fw_chunk.fw_length;

                        // 270 bytes /512 so from 0-269 it holds space

                    } else {
                        ESP_LOGE(HTTP_TAG, "ERROR: PREV_FW_LENGTH IS A NEGATIVE NUMBER");
                    }

                    printf("the first %02x %02x %02x %02x\n\r", fw_chunk.fw_img[0],fw_chunk.fw_img[1],fw_chunk.fw_img[2],fw_chunk.fw_img[3]);
                    printf("the cipher %02x %02x %02x %02x\n\r", fw_chunk.fw_img[24],fw_chunk.fw_img[25],fw_chunk.fw_img[26],fw_chunk.fw_img[27]);
                    total_fw_bytes = fw_chunk.fw_length;

                    memcpy(magic, package, MAGIC_LEN);
                    magic[4] = '\0';

                    fw_size = ((uint32_t)package[4] << 24) |
                              ((uint32_t)package[5] << 16) |
                              ((uint32_t)package[6] << 8 ) |
                              ((uint32_t)package[7] << 0 );

                    ESP_LOGI(HTTP_TAG, "magic: %s and firmware size: %d", magic, fw_size);

                    memcpy(iv, &package[8], IV_LEN);

                    mbedtls_aes_init(&aes_ctx);
                    int ret = mbedtls_aes_setkey_enc(&aes_ctx, AES_KEY, 256);  // 256-bit key
                    if (ret != 0) {
                        ESP_LOGE(HTTP_TAG, "AES setkey failed: %d", ret);
                    } else {
                        memcpy(ctr_nonce_counter, iv, IV_LEN);   // initial counter = IV
                        memset(stream_block, 0, sizeof(stream_block));
                        nc_off = 0;
                        aes_ready = true;
                        ESP_LOGI(HTTP_TAG, "AES-CTR context initialised");
                    }


                    eofpackage = 1;
                    // after this, remaining fw_chunk.fw_img bytes (if any) are firmware body
                    // subsequent packets will hit the eofpackage && eofheader path

                    ESP_LOGI(HTTP_TAG, "Package parsed out!");
                }

                // HTTP PARSING
                if (!eofheader) {
                     printf("the first %02x %02x %02x %02x\n\r", http_buffer[0],http_buffer[1],http_buffer[2],http_buffer[3]);
                    for (int i = 3; i < received_http; ++i) {
                        if (http_buffer[i]   == '\n' &&
                            http_buffer[i-1] == '\r' &&
                            http_buffer[i-2] == '\n' &&
                            http_buffer[i-3] == '\r') {

                            int header_end = i + 1;  // index just after \r\n\r\n
                            int body_bytes = received_http - header_end;
                            if (body_bytes < 0) body_bytes = 0;

                            prev_fw_len = body_bytes;

                            if (body_bytes > 0) {
                                memcpy(&fw_chunk.fw_img[0], &http_buffer[header_end], body_bytes);
                            }

                            eofheader = 1;
                            break;
                        }
                    }

                    ESP_LOGI(HTTP_TAG, "HTTP Header Parsed out");
                }

            } while (1);


            ESP_LOGI(HTTP_TAG, "done reading from socket.");
            close(sock);

            while (!task_send_done ) {
                 vTaskDelay(pdMS_TO_TICKS(100));
            }

            ESP_LOGI(HTTP_TAG, "Starting again! and we got end_signal");
            task_send_done  = 0;   // reset if you plan to support another update
            b_signal = 0;
        }
    }
}

void taskSendFirmware(void *pvParameter){

    fw_chunk_t fw_chunk;
    int id = 0;
    uint8_t flash_write = 0;

    for(;;){

        //int len = uart_read_bytes(UART_NUM_0, &b_signal, 1, pdMS_TO_TICKS(5000));
        if(xQueueReceive(qBuffer, &fw_chunk, portMAX_DELAY)){
            if(fw_chunk.fw_length || fw_chunk.last_chunk){


                ESP_LOGI(UART_TAG, "before decryption %02x %02x %02x %02x\n\r", fw_chunk.fw_img[0],fw_chunk.fw_img[1],fw_chunk.fw_img[2],fw_chunk.fw_img[3]);


                if (aes_ready && fw_chunk.fw_length > 0) {
                    int ret = mbedtls_aes_crypt_ctr(
                        &aes_ctx,
                        fw_chunk.fw_length,      // length of this chunk
                        &nc_off,                 // will be updated
                        ctr_nonce_counter,       // counter state
                        stream_block,            // keystream buffer
                        fw_chunk.fw_img,         // input (ciphertext)
                        fw_chunk.fw_img          // output (plaintext in place)
                    );
                    if (ret != 0) {
                        ESP_LOGE(UART_TAG, "AES-CTR decrypt failed: %d", ret);
                        continue;   // don't send bad data
                    }
                }


                do{
                    int n = uart_write_bytes(UART_NUM_1, (const char *) &fw_chunk, sizeof(fw_chunk));
                    if (n == sizeof(fw_chunk)) vTaskDelay(10);
                }while(!uart_read_bytes(UART_NUM_0, &flash_write, 1, 0));

                    // int n = uart_write_bytes(UART_NUM_1, (const char *) &fw_chunk, sizeof(fw_chunk));
                 if (fw_chunk.last_chunk) {
                    task_send_done  = 1;   // all firmware has been sent
                }

                ESP_LOGI(UART_TAG, "SENDTASK - first i: %02X, length: %d, last chunk: %d and id(%d)",
                            fw_chunk.fw_img[0], fw_chunk.fw_length, fw_chunk.last_chunk, id);
                ESP_LOGI(UART_TAG, "%02X\n\r", fw_chunk.fw_img[0]);
                ESP_LOGI(UART_TAG, "%02X\n\r", fw_chunk.fw_img[1]);
                ESP_LOGI(UART_TAG, "%02X\n\r", fw_chunk.fw_img[2]);
                ESP_LOGI(UART_TAG, "%02X\n\r", fw_chunk.fw_img[3]);
                ESP_LOGI(UART_TAG, "%02X\n\r", fw_chunk.fw_img[4]);
                uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(50));

                ESP_LOGI(UART_TAG, "out of loop");

                ESP_LOGI(UART_TAG, "received flash_write signal from id(%d)", id);
                id++;
            }
        }

    }

}
