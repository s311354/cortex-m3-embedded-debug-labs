cmake_minimum_required(VERSION 3.20.0)

# Allow Zephyr to find this lab's local DT binding
list(APPEND DTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(lab22_spi_command_read_zephyr LANGUAGES C)

target_sources(app PRIVATE
	src/main.c
	src/spi_command.c
	drivers/clock_control/lab22_fixed_clock.c
)

target_include_directories(app PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
)
