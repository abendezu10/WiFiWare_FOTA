cmake_minimum_required(VERSION 3.23)
project(WiFiWare)

add_subdirectory(bcb)

# Let user point to CubeIDE project once
set(STM32_PROJECT_DIR ""
    CACHE PATH "Path to STM32CubeIDE project directory")

if(NOT STM32_PROJECT_DIR)
    message(WARNING "STM32_PROJECT_DIR is not set yet")
endif()

# Target to ONLY flip active slot + regenerate linker script
add_custom_target(flip_slot
    COMMAND "${CMAKE_COMMAND}"
            -DSTM32_PROJECT_DIR="${STM32_PROJECT_DIR}"
            -P "${CMAKE_SOURCE_DIR}/bcb/bcb_activeswitch.cmake"
    COMMENT "Flipping active slot and regenerating STM32 linker script"
)
