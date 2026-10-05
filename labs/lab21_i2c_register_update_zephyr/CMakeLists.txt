cmake_minimum_required(VERSION 3.20.0)

# Allow Zephyr to find this lab's local DT binding
list(APPEND DTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(lab21_i2c_register_update_zephyr LANGUAGES C)

target_sources(app PRIVATE
	src/main.c
	src/reg_access.c
)

target_include_directories(app PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
)
