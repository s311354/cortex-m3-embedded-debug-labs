cmake_minimum_required(VERSION 3.20.0)

# Allow Zephyr to find this lab's local DT binding
list(APPEND DTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

set(LAB24_EEPROM_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/state/fault-eeprom.bin"
	CACHE FILEPATH "Persistent 512-byte QEMU EEPROM image")

get_filename_component(LAB24_EEPROM_IMAGE "${LAB24_EEPROM_IMAGE}" ABSOLUTE
	BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(LAB24_QEMU_I2C_BUS "/versatile_i2c/i2c" CACHE STRING
	"QEMU I2C bus: verify it belongs to MMIO 0x4002a000 using info qtree")

# Consumed by Zephyr's existing QEMU runner
list(APPEND QEMU_EXTRA_FLAGS
	-drive "if=none,id=lab24_store,format=raw,cache=writethrough,file=${LAB24_EEPROM_IMAGE}"
	-device "at24c-eeprom,id=lab24_eeprom,bus=${LAB24_QEMU_I2C_BUS},address=0x50,rom-size=512,address-size=1,writable=on,drive=lab24_store")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(lab24_eeprom_fault_logger_zephyr LANGUAGES C)

target_sources(app PRIVATE
	src/main.c
	src/fault_log.c
)

target_include_directories(app PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
)
