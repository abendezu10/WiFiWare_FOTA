#include <stdint.h>

typedef enum {
    SLOT_B = 0,
    SLOT_A = 1
} slot_t;

typedef struct __attribute__((packed)) {
    uint32_t active_slot;
} bcb_t;

__attribute__((used, section(".bcb")))
const bcb_t bcb = {
    .active_slot = SLOT_A
};
