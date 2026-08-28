#ifndef LAB19_PCIE_RESOURCE_H
#define LAB19_PCIE_RESOURCE_H

#include <stddef.h>
#include <stdint.h>

#include "pcie_host.h"

struct lab_pcie_resource_window {
    uintptr_t pci_base;
    uintptr_t cpu_base;

    size_t size;

    uintptr_t next;
};

struct lab_pcie_bar_resource {
    unsigned int bar_index;

    uint32_t original;

    uintptr_t bus_addr;
    uintptr_t cpu_addr;

    size_t size;

    uint8_t is_io;
    uint8_t is_64bit;
    uint8_t prefetchable;
};

int lab_pcie_resource_window_init(struct lab_pcie_resource_window *window, uintptr_t pci_base, uintptr_t cpu_base, size_t size);

int lab_pcie_resource_alloc(struct lab_pcie_resource_window *window, size_t size, size_t alignment, uintptr_t *pci_addr);

int lab_pcie_resource_translate(const struct lab_pcie_resource_window *window, uintptr_t pci_addr, uintptr_t *cpu_addr);

int lab_pcie_probe_bar32(struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int bar_index, struct lab_pcie_bar_resource *bar);

int lab_pcie_assign_bar32(struct lab_pcie_host *host, pcie_bdf_t bdf, struct lab_pcie_resource_window *window, struct lab_pcie_bar_resource *bar);

int lab_pcie_enable_memory_space(struct lab_pcie_host *host, pcie_bdf_t bdf);

#endif
