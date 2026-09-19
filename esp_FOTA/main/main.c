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
        ESP_LOGI(UART_TAG, "read result: len=%d b_signal=%d", len, b_signal);
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


            int header_done = 0, package_done = 0;
            int track = 0;
            size_t pkg_have = 0;
            uint32_t total_fw_bytes = 0;

            fw_chunk_t fw_chunk = {.fw_length = 0, .last_chunk = 0};
            uint8_t http_buffer[FW_BUFFER_SIZE];

            for(;;){
                received_http = read(sock, http_buffer, sizeof(http_buffer));

                if(received_http < 0){
                    ESP_LOGE(HTTP_TAG, "socket read failed at %d of %d bytes", total_fw_bytes, fw_size);
                    break;
                }

                if(received_http == 0){
                    if(fw_chunk.fw_length > 0){
                        fw_chunk.last_chunk = 1;
                        xQueueSend(qBuffer, &fw_chunk, portMAX_DELAY);
                        ESP_LOGI(HTTP_TAG, "EOF flush of %d bytes now being sent to queue [%d]", fw_chunk.fw_length, track);
                        track++;
                    } else {
                        ESP_LOGE(HTTP_TAG, "EOF at %d of %d bytes, image is incomplete", total_fw_bytes, fw_size);
                    }
                    break;
                }

                ESP_LOGI(HTTP_TAG, "Received HTTP: %d [%d]", received_http, track);

                size_t offset = 0;
                size_t avail = (size_t)received_http;

                if(!header_done){
                    for(size_t i = 3; i < avail; ++i){
                        if(http_buffer[i]   == '\n' &&
                           http_buffer[i-1] == '\r' &&
                           http_buffer[i-2] == '\n' &&
                           http_buffer[i-3] == '\r'){

                            offset += i + 1;
                            avail  -= i + 1;
                            header_done = 1;
                            break;
                        }
                    }

                    if(!header_done) continue;

                    ESP_LOGI(HTTP_TAG, "HTTP Header Parsed out");
                }

                if(!package_done){
                    size_t take = PACKAGE_LEN - pkg_have;
                    if(take > avail) take = avail;

                    memcpy(&package[pkg_have], &http_buffer[offset], take);
                    pkg_have += take;
                    offset   += take;
                    avail    -= take;

                    if(pkg_have < PACKAGE_LEN) continue;

                    memcpy(magic, package, MAGIC_LEN);
                    magic[4] = '\0';

                    fw_size = ((uint32_t)package[4] << 24) |
                              ((uint32_t)package[5] << 16) |
                              ((uint32_t)package[6] << 8 ) |
                              ((uint32_t)package[7] << 0 );

                    memcpy(iv, &package[8], IV_LEN);

                    mbedtls_aes_init(&aes_ctx);
                    int ret = mbedtls_aes_setkey_enc(&aes_ctx, AES_KEY, 256);
                    if(ret != 0){
                        ESP_LOGE(HTTP_TAG, "AES setkey failed: %d", ret);
                        break;
                    }

                    memcpy(ctr_nonce_counter, iv, IV_LEN);
                    memset(stream_block, 0, sizeof(stream_block));
                    nc_off = 0;
                    aes_ready = true;
                    package_done = 1;

                    ESP_LOGI(HTTP_TAG, "magic: %s and firmware size: %d", magic, fw_size);
                    ESP_LOGI(HTTP_TAG, "Package parsed out!");
                }

                while(avail > 0){
                    size_t space = FW_BUFFER_SIZE - fw_chunk.fw_length;
                    size_t take  = (avail < space) ? avail : space;

                    memcpy(&fw_chunk.fw_img[fw_chunk.fw_length], &http_buffer[offset], take);
                    fw_chunk.fw_length += take;
                    total_fw_bytes     += take;
                    offset += take;
                    avail  -= take;

                    if(total_fw_bytes >= fw_size) fw_chunk.last_chunk = 1;

                    if(fw_chunk.fw_length == FW_BUFFER_SIZE || fw_chunk.last_chunk){
                        if(xQueueSend(qBuffer, &fw_chunk, portMAX_DELAY) != pdPASS){
                            ESP_LOGI(HTTP_TAG, "Sending firmware buffer to the Send Task has failed!");
                        } else {
                            ESP_LOGI(HTTP_TAG, "The bytes received are: %d and now being sent to queue [%d]", fw_chunk.fw_length, track);
                            track++;
                        }

                        if(fw_chunk.last_chunk) break;

                        fw_chunk.fw_length = 0;
                        memset(fw_chunk.fw_img, 0, FW_BUFFER_SIZE);
                    }
                }

                if(fw_chunk.last_chunk) break;
            }


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

                ESP_LOGI(UART_TAG, "Decryption success!");
                
                ESP_LOGI(UART_TAG, "after decryption %02x %02x %02x %02x\n\r", fw_chunk.fw_img[0],fw_chunk.fw_img[1],fw_chunk.fw_img[2],fw_chunk.fw_img[3]);
                /* The fw_chunk is correctly decrypted but once it goes
                 *
                 *
                 */

                int m = 0;
                int retries = 0;
                fw_chunk.crc = crc32_le(0, fw_chunk.fw_img, fw_chunk.fw_length);
                do{
                    ESP_LOGI(UART_TAG, "Sending to STM32- first i: %02X, length: %d, last chunk: %d and id(%d)",
                        fw_chunk.fw_img[0], fw_chunk.fw_length, fw_chunk.last_chunk, id);

                    int n = uart_write_bytes(UART_NUM_1, (const char *) &fw_chunk, sizeof(fw_chunk));
                    printf("the value of n= %d\n", n);
                    if (n == sizeof(fw_chunk)) vTaskDelay(10);
                    m = uart_read_bytes(UART_NUM_0, &flash_write, 1, 0);
                    printf("the value of m= %d\n", m);
                    retries++;
                }while(!m && retries < 20);

                if(!m){
                    ESP_LOGE(UART_TAG, "no ACK for id(%d) after %d tries, aborting update", id, retries);
                    task_send_done = 1;
                    continue;
                }

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
