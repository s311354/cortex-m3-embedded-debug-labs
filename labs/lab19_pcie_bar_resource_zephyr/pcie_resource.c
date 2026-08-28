#include <stddef.h>
#include <stdint.h>

#include "pcie_resource.h"

static uintptr_t align_up(uintptr_t value, size_t alignment) {
    uintptr_t mask;

    mask = (uintptr_t) alignment - 1U;

    return (value + mask) & ~mask;
}

int lab_pcie_resource_window_init(struct lab_pcie_resource_window *window, uintptr_t pci_base, uintptr_t cpu_base, size_t size) {
    if ((window == NULL) || (size == 0U))
        return LAB_PCIE_ERR_ARGUMENT;

    window->pci_base = pci_base;
    window->cpu_base = cpu_base;
    window->size = size;
    window->next = pci_base;

    return LAB_PCIE_OK;
}

int lab_pcie_resource_alloc(struct lab_pcie_resource_window *window, size_t size, size_t alignment, uintptr_t *pci_addr) {
    uintptr_t start;
    uintptr_t limit;

    if ((window == NULL) || (pci_addr == NULL) || (size == 0U) || (alignment == 0U)) {
        return LAB_PCIE_ERR_ARGUMENT;
    }

    if ((alignment & (alignment - 1U)) != 0U)
	return LAB_PCIE_ERR_ARGUMENT;


    start = align_up(window->next, alignment);

    limit = window->pci_base + window->size;

    if ((start < window->pci_base) || (start > limit) || (size > (size_t)(limit - start)))
	return LAB_PCIE_ERR_NO_RESOURCE;

    *pci_addr = start;

    window->next = start + size;

    return LAB_PCIE_OK;
}

int lab_pcie_resource_translate(const struct lab_pcie_resource_window *window, uintptr_t pci_addr, uintptr_t *cpu_addr) {
    uintptr_t offset;

    if ((window == NULL) || (cpu_addr == NULL))
	return LAB_PCIE_ERR_ARGUMENT;

    if ((pci_addr < window->pci_base) || (pci_addr >= window->pci_base + window->size)) {
        return LAB_PCIE_ERR_NO_RESOURCE;
    }

    offset = pci_addr - window->pci_base;

    *cpu_addr = window->cpu_base + offset;

    return LAB_PCIE_OK;
}

int lab_pcie_probe_bar32(struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int bar_index, struct lab_pcie_bar_resource *bar) {
    unsigned int reg;

    uint32_t original;
    uint32_t probe;
    uint32_t address_mask;

    int result;

    if ((host == NULL) || (bar == NULL) || (bar_index >= 6U)) {
        return LAB_PCIE_ERR_ARGUMENT;
    }

    reg = PCIE_CONF_BAR0 + bar_index;

    result = lab_pcie_config_read32(host, bdf, reg, &original);

    if (result != LAB_PCIE_OK)
	return result;

    bar->bar_index = bar_index;
    bar->original = original;

    bar->bus_addr = 0U;
    bar->cpu_addr = 0U;
    bar->size = 0U;

    bar->is_io = PCIE_CONF_BAR_IO(original) ? 1U : 0U;

    bar->is_64bit = (PCIE_CONF_BAR_MEM(original) && PCIE_CONF_BAR_64(original)) ? 1U : 0U;

    bar->prefetchable = (PCIE_CONF_BAR_MEM(original) && ((original & 0x8U) != 0U)) ? 1U : 0U;

    /*lab 19 scope: detect 64-bit BAR but do not assign it yet*/
    if (bar->is_64bit != 0U)
	return LAB_PCIE_ERR_UNSUPPORTED;

    result = lab_pcie_config_write32(host, bdf, reg, 0xFFFFFFFFU);

    if (result != LAB_PCIE_OK)
	return result;

    result = lab_pcie_config_read32(host, bdf, reg, &probe);

    /*
     * Restore the original BAR regardless of probe result
     */
    (void)lab_pcie_config_write32(host, bdf, reg, original);

    if (result != LAB_PCIE_OK)
	return result;

    if (bar->is_io != 0U)
	address_mask = PCIE_CONF_BAR_IO_ADDR(probe);
    else 
	address_mask = PCIE_CONF_BAR_ADDR(probe);

    if (address_mask == 0U) {
        bar->size = 0U;
	return LAB_PCIE_OK;
    }

    bar->size = (size_t) ((~address_mask) + 1U);

    return LAB_PCIE_OK;
}

int lab_pcie_assign_bar32(struct lab_pcie_host *host, pcie_bdf_t bdf, struct lab_pcie_resource_window *window, struct lab_pcie_bar_resource *bar) {
    uintptr_t bus_addr;
    uintptr_t cpu_addr;

    uint32_t programmed;

    unsigned int reg;

    int result;

    if ((host == NULL) || (window == NULL) || (bar == NULL) || (bar->size == 0U))
	return LAB_PCIE_ERR_UNSUPPORTED;

    result = lab_pcie_resource_alloc(window, bar->size, bar->size, &bus_addr);

    if (result != LAB_PCIE_OK)
	return result;

    result = lab_pcie_resource_translate(window, bus_addr, &cpu_addr);

    if (result != LAB_PCIE_OK)
	return result;

    reg = PCIE_CONF_BAR0 + bar->bar_index;

    if (bar->is_io != 0U) {
        programmed = ((uint32_t) bus_addr & ~0x3U) | (bar->original & 0x3U);
    } else {
        programmed = ((uint32_t) bus_addr & ~0xFU) | PCIE_CONF_BAR_FLAGS(bar->original);
    }

    result = lab_pcie_config_write32(host, bdf, reg, programmed);

    if (result != LAB_PCIE_OK)
	return result;

    bar->bus_addr = bus_addr;
    bar->cpu_addr = cpu_addr;

    return LAB_PCIE_OK;
}

int lab_pcie_enable_memory_space(struct lab_pcie_host *host, pcie_bdf_t bdf) {
    uint32_t command_status;
    
    int result;

    result = lab_pcie_config_read32(host, bdf, PCIE_CONF_CMDSTAT, &command_status);

    if (result != LAB_PCIE_OK)
	return result;

    command_status |= PCIE_CONF_CMDSTAT_MEM;

    return lab_pcie_config_write32(host, bdf, PCIE_CONF_CMDSTAT, command_status);
}
