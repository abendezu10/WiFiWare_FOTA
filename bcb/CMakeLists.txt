cmake_minimum_required(VERSION 3.23)
project(bcb_image C)

# Adjust if you want explicit full paths
set(ARM_GCC     arm-none-eabi-gcc)
set(ARM_OBJCOPY arm-none-eabi-objcopy)

set(BCB_ELF "${CMAKE_CURRENT_BINARY_DIR}/bcb.elf")
set(BCB_BIN "${CMAKE_CURRENT_BINARY_DIR}/bcb.bin")

# 1) Build the ELF using the ARM cross-compiler
add_custom_command(
    OUTPUT "${BCB_ELF}"
    COMMAND "${ARM_GCC}"
            -nostartfiles
            -nostdlib
            -Wl,-T,"${CMAKE_CURRENT_SOURCE_DIR}/link.ld"
            -Wl,--gc-sections
            -o "${BCB_ELF}"
            "${CMAKE_CURRENT_SOURCE_DIR}/bcb.c"
    DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/bcb.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/link.ld"
    COMMENT "Building BCB ELF with arm-none-eabi-gcc"
)

# 2) Convert ELF -> BIN with arm-none-eabi-objcopy
add_custom_command(
    OUTPUT "${BCB_BIN}"
    COMMAND "${ARM_OBJCOPY}" -O binary "${BCB_ELF}" "${BCB_BIN}"
    DEPENDS "${BCB_ELF}"
    COMMENT "Generating bcb.bin"
)

# 3) Custom target 'bcb' that just builds the bin
add_custom_target(bcb
    DEPENDS "${BCB_BIN}"
    COMMENT "BCB image ready"
)

# 4) Optional: 'flash_bcb' target to actually flash it
add_custom_target(flash_bcb
    DEPENDS bcb
    COMMAND st-flash --reset write "${BCB_BIN}" 0x08008000
    COMMENT "Flashing bcb.bin to 0x08008000"
)
