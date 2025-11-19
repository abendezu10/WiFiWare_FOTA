# bcb/bcb_activeslot.cmake

if(NOT DEFINED STM32_PROJECT_DIR OR STM32_PROJECT_DIR STREQUAL "")
    message(FATAL_ERROR "STM32_PROJECT_DIR is not set")
endif()

# CMAKE_CURRENT_LIST_DIR is where THIS script lives: WiFiWare/bcb
set(BCB_DIR "${CMAKE_CURRENT_LIST_DIR}")

# WiFiWare root is parent of bcb/
get_filename_component(ROOT_DIR "${BCB_DIR}/.." ABSOLUTE)

# Read current active slot
file(READ "${BCB_DIR}/bcb_activeslot.txt" ACTIVE_SLOT_RAW)
string(STRIP "${ACTIVE_SLOT_RAW}" ACTIVE_SLOT_STR)

if(ACTIVE_SLOT_STR STREQUAL "SLOT_A")
    set(FLASH_ORIGIN 0x08040000)
    set(FLASH_LENGTH 256K)

    # Flip to SLOT_B
    file(WRITE "${BCB_DIR}/bcb_activeslot.txt" "SLOT_B")

    configure_file(
        "${ROOT_DIR}/STM32F401RETX_FLASH.ld.temp"
        "${STM32_PROJECT_DIR}/STM32F401RETX_FLASH.ld"
        @ONLY
    )

elseif(ACTIVE_SLOT_STR STREQUAL "SLOT_B")
    set(FLASH_ORIGIN 0x0800C000)
    set(FLASH_LENGTH 208K)

    # Flip to SLOT_A
    file(WRITE "${BCB_DIR}/bcb_activeslot.txt" "SLOT_A")

    configure_file(
        "${ROOT_DIR}/STM32F401RETX_FLASH.ld.temp"
        "${STM32_PROJECT_DIR}/STM32F401RETX_FLASH.ld"
        @ONLY
    )

else()
    message(FATAL_ERROR
        "Invalid ACTIVE_SLOT value '${ACTIVE_SLOT_STR}' (expected SLOT_A or SLOT_B)")
endif()

message(STATUS "Active slot was ${ACTIVE_SLOT_STR}; linker script regenerated.")
