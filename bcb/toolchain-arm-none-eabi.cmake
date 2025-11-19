# toolchain-arm-none-eabi.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)

# Avoid CMake trying to run test executables on the host
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)