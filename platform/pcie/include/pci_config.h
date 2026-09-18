#ifndef PCI_CONFIG_H
#define PCI_CONFIG_H

#include <stdint.h>

/*
 * =========================================
 * Type-0 PCI Configuration Header offsets
 * =========================================
 */

#define PCI_CFG_VENDOR_DEVICE         0x000U
#define PCI_CFG_COMMAND_STATUS        0x004U

#define PCI_CFG_BAR0                  0x010U
#define PCI_CFG_BAR1                  0x014U
#define PCI_CFG_BAR2                  0x018U
#define PCI_CFG_BAR3                  0x01CU
#define PCI_CFG_BAR4                  0x020U
#define PCI_CFG_BAR5                  0x024U

/*
 * =========================================
 * Command Register
 * =========================================
 */

#define PCI_COMMAND_IO_ENABLE         (1U << 0)
#define PCI_COMMAND_MEMORY_ENABLE     (1U << 1)
#define PCI_COMMAND_BUS_MASTER_ENABLE (1U << 2)

/*
 * =========================================
 * BAR encoding
 * =========================================
 */

#define PCI_BAR_IO_SPACE              (1U << 0)

#define PCI_BAR_MEM_TYPE_MASK         (3U << 1)

#define PCI_BAR_MEM_TYPE_32           (0U << 1)
#define PCI_BAR_MEM_TYPE_64           (2U << 1)

#define PCI_BAR_MEM_PREFETCH          (1U << 3)

#define PCI_BAR_MEM_ADDR_MASK         0xFFFFFFF0UL

#endif
