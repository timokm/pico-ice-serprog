cmake_minimum_required(VERSION 3.13)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include(pico_sdk_import.cmake)
project(picoice_serprog)

pico_sdk_init()

add_executable(picoice_serprog)
target_sources(picoice_serprog PRIVATE main.c usb_descriptors.c ice_fpga_flash.c)
target_link_libraries(picoice_serprog PRIVATE pico_stdlib hardware_spi tinyusb_device)
pico_add_extra_outputs(picoice_serprog)
