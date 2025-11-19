# Creating BCB structure in 0x08008000

cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-arm-none-eabi.cmake

rm -rf build
cmake -S . -B build \
  -DSTM32_PROJECT_DIR="/mnt/c/Users/abend/STM32CubeIDE/workspace_1.18.1/temp1"

# Only flip slot + regen linker script
cmake --build build --target flip_slot

# Build + flash BCB struct (sector 2)
cmake --build build --target bcb