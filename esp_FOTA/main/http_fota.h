#ifndef INC_HTTP_FOTA_
#define INC_HTTP_FOTA_

#define WEB_SERVER              "192.168.1.152"
#define WEB_PORT                8000
#define WEB_PATH                "/firmware/firmware_encrypted.bin"

#define FW_BUFFER_SIZE             (512) // bytes
#define FW_QUEUE_SIZE               (8)

typedef struct __attribute__((packed)) {
    uint16_t fw_length;       // fixed width, little-endian
    uint8_t  last_chunk;      // 0/1
    uint8_t  fw_img[FW_BUFFER_SIZE];
} fw_chunk_t;

#endif
