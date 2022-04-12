cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(udp_neopixel_driver C CXX ASM)

pico_sdk_init()

# RMII Library
add_subdirectory("lib/rp2040-rmii")

# rest of your project
add_executable(udp_neopixel_driver
    src/main.c
    src/udp2pixel.c
)

pico_generate_pio_header(udp_neopixel_driver ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_link_libraries(udp_neopixel_driver pico_stdlib rp2040_rmii hardware_pio hardware_dma hardware_irq)

# enable usb output, disable uart output
pico_enable_stdio_usb(udp_neopixel_driver 1)
pico_enable_stdio_uart(udp_neopixel_driver 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(udp_neopixel_driver)
