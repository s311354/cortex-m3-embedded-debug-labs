#ifndef M3DS_PCIE_HOST_REGS_H
#define M3DS_PCIE_HOST_REGS_H

#include <stddef.h>
#include <stdint.h>

#define M3DS_PCIE_HOST_BASE            0xA0000000UL
#define M3DS_PCIE_HOST_SIZE            0x00010000UL

#define M3DS_PCIE_VERSION_VALUE        0x00010000UL

#define M3DS_PCIE_CONTROL_ENABLE       (1UL << 0)

#define M3DS_PCIE_STATUS_BUSY          (1UL << 0)
#define M3DS_PCIE_STATUS_DONE          (1UL << 1)
#define M3DS_PCIE_STATUS_ERROR         (1UL << 2)

#define M3DS_PCIE_CMD_CFG_READ          1UL
#define M3DS_PCIE_CMD_CFG_WRITE         2UL

#define M3DS_PCIE_ERR_DISABLED          (1UL << 0)
#define M3DS_PCIE_ERR_BAD_COMMAND       (1UL << 1)
#define M3DS_PCIE_ERR_BACKEND           (1UL << 2)
#define M3DS_PCIE_ERR_BUSY              (1UL << 3)
#define M3DS_PCIE_ERR_BAD_ACCESS        (1UL << 4)

#define M3DS_PCIE_BDF(bus, dev, fn) \
	((((uint32_t)(bus)  & 0xFFU) << 16U)  | \
	 (((uint32_t)(dev)  & 0x1FU) << 11U)  | \
	 (((uint32_t)(fn)   & 0x07U) << 8U))

struct m3ds_pcie_host_regs {
    volatile uint32_t version;      // 0x00
    volatile uint32_t control;      // 0x04
    volatile uint32_t status;       // 0x08

    uint32_t reserved0;             // 0x0C

    volatile uint32_t cfg_bdf;      // 0x10
    volatile uint32_t cfg_reg;      // 0x14
    volatile uint32_t cfg_wdata;    // 0x18
    volatile uint32_t cfg_rdata;    // 0x1C
    volatile uint32_t cfg_command;  // 0x20
    volatile uint32_t error_status; // 0x24
};

_Static_assert(
		offsetof(struct m3ds_pcie_host_regs, cfg_bdf) == 0x10U,
		"CFG_BDF offset mismatch"
	      );

_Static_assert(
		offsetof(struct m3ds_pcie_host_regs, cfg_command) == 0x20U,
		"CFG_COMMAND offset mismatch"
	      );

_Static_assert(
		offsetof(struct m3ds_pcie_host_regs, error_status) == 0x24U,
		"ERROR_STATUS offset mismatch"
	      );

#endif
