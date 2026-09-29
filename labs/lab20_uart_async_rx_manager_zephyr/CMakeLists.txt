cmake_minimum_required(VERSION 3.20.0)

#
# Allow Zephyr to find the devicetree binding under:
#    dts/bindings/serial/
#
list(APPEND DTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
option(QEMU_PTY "Use a PTY for the Lab20 QEMU UART" ON)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lab20_uart_async_rx_manager_zephyr LANGUAGES C)

target_sources(app PRIVATE
	src/main.c
	src/rx_manager.c
)

if (CONFIG_LAB20_CMSDK_UART_ASYNC)
	target_sources(app PRIVATE drivers/uart/uart_cmsdk_async.c)
endif()

target_include_directories(app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
