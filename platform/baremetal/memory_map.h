/**
 * @file    memory_map.h
 * @brief   Memory map definitions extracted from CM3DS_MPS2.h for linker script use
 * @details This header provides ONLY memory-related definitions needed by linker scripts.
 *          It avoids including full device headers which contain C-specific constructs
 *          incompatible with GNU ld linker script syntax.
 * 
 * Platform: ARM MPS2 AN385 (Cortex-M3 DesignStart)
 */

#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/*
 * These values are derived from CM3DS_MPS2.h and represent the
 * FIXED hardware memory layout of the ARM MPS2 AN385 platform.
 * 
 * DO NOT MODIFY these values unless you are targeting different hardware.
 * 
 * Note: No UL suffix - linker scripts use bare hex values
 */

/* Flash Memory (Code Region) */
#define CM3DS_MPS2_FLASH_BASE        0x00000000  /*!< FLASH base address */
#define CM3DS_MPS2_FLASH_SIZE        0x00040000  /*!< 256 KB (0x40000) */

/* SRAM (Data Region) */
#define CM3DS_MPS2_SRAM_BASE         0x20000000  /*!< SRAM base address */
#define CM3DS_MPS2_SRAM_SIZE         0x00020000  /*!< 64 KB (0x20000) */

/* Convenience aliases */
#define CM3DS_MPS2_RAM_BASE          CM3DS_MPS2_SRAM_BASE
#define CM3DS_MPS2_RAM_SIZE          CM3DS_MPS2_SRAM_SIZE

/*
 * Hardware Verification
 * These calculations allow compile-time verification of memory layout
 */
#define CM3DS_MPS2_FLASH_END         (CM3DS_MPS2_FLASH_BASE + CM3DS_MPS2_FLASH_SIZE)
#define CM3DS_MPS2_SRAM_END          (CM3DS_MPS2_SRAM_BASE + CM3DS_MPS2_SRAM_SIZE)

/* Peripheral Memory (not used in baremetal linker script, but documented here) */
#define CM3DS_MPS2_PERIPH_BASE       0x40000000  /*!< Peripheral base */
#define CM3DS_MPS2_APB_BASE          0x40000000  /*!< APB bus base */
#define CM3DS_MPS2_AHB_BASE          0x40010000  /*!< AHB bus base */

#endif /* MEMORY_MAP_H */
