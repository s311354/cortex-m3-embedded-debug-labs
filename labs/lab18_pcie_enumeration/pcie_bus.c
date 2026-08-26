
#include <stddef.h>
#include <stdint.h>

#include "pcie_bus.h"

static int pcie_read32(struct pcie_bus *bus, struct pcie_bdf bdf, uint16_t offset, uint32_t *value) {
    return pcie_config_read32(bus->host, bdf, offset, value);
}

static void pcie_decode_bar(struct pcie_bar *bar, uint32_t raw) {
    if (bar == NULL)
	return;

    bar->raw = raw;

    /*
     * I/O BAR
     */
    if ((raw & 0x1U) != 0U) {
        bar->is_io = 1U;
	bar->is_64bit = 0U;
	bar->prefetchable = 0U;

	bar->address = raw & 0xFFFFFFFCU;

	return;
    }

    /*
     * Memory BAR
     */
    bar->is_io = 0U;

    /*
     * Memory BAR type
     */
    bar->is_64bit = (((raw >> 1U) & 0x3U) == 0x2U) ? 1U : 0U;

    bar->prefetchable = ((raw & 0x8U) != 0U) ? 1U : 0U;

    bar->address = raw & 0xFFFFFFF0U;
}

static int pcie_read_function(struct pcie_bus *bus, struct pcie_bdf bdf, struct pcie_device *dev) {
    uint32_t value;
    unsigned int index;
    int result;

    if ((bus == NULL) || (dev == NULL)) {
        return PCIE_ERR_ARGUMENT;
    }

    result = pcie_read32(bus, bdf, PCIE_CFG_VENDOR_DEVICE, &value);

    if (result != PCIE_OK)
        return result;

    dev->bdf = bdf;

    dev->vendor_id = (uint16_t) (value & 0xFFFFU);

    dev->device_id = (uint16_t) (value >> 16U);

    result = pcie_read32(bus, bdf, PCIE_CFG_CLASS_REVISION, &value);

    if (result != PCIE_OK)
	return result;

    dev->revision = (uint8_t)(value & 0xFFU);

    dev->subclass = (uint8_t)((value >> 16U) & 0xFFU);

    dev->class_code = (uint8_t)(value >> 24U & 0xFFU);

    result = pcie_read32(bus, bdf, PCIE_CFG_HEADER, &value);

    if (result != PCIE_OK)
	return result;

    dev->header_type = (uint8_t)((value >> 16U) & 0x7FU);

    dev->multifunction = ((value & 0x00800000U) != 0U) ? 1U : 0U;

    if (dev->header_type == PCIE_HEADER_TYPE_NORMAL) {
        for (index = 0U; index < PCIE_MAX_BARS; ++index) {
	    result = pcie_read32(bus, bdf, (uint16_t)(PCIE_CFG_BAR0 + (index * 4U)), &value);

	    if (result != PCIE_OK)
		return result;

	    pcie_decode_bar(&dev->bars[index], value);
	}
    }

    result = pcie_read32(bus, bdf, PCIE_CFG_INTERRUPT, &value);

    if (result != PCIE_OK)
	return result;

    dev->irq_line = (uint8_t) (value & 0xFFU);

    dev->irq_pin = (uint8_t)((value >> 8U) & 0xFFU);

    return PCIE_OK;
}

static int pcie_scan_function(struct pcie_bus *bus, uint8_t bus_number, uint8_t device_number, uint8_t function_number) {
    struct pcie_bdf bdf;
    struct pcie_device *dev;

    uint32_t id;
    uint16_t vendor;

    int result;

    bdf.bus = bus_number;
    bdf.device = device_number;
    bdf.function = function_number;

    result = pcie_read32(bus, bdf, PCIE_CFG_VENDOR_DEVICE, &id);

    if (result != PCIE_OK)
	return result;

    vendor = (uint16_t)(id & 0xFFFFU);

    if (vendor == 0xFFFFU)
	return PCIE_OK;

    if (bus->device_count >= PCIE_MAX_DEVICES) {
        return PCIE_ERR_ARGUMENT;
    }

    dev = &bus->devices[bus->device_count];

    result = pcie_read_function(bus, bdf, dev);

    if (result != PCIE_OK)
	return result;

    ++bus->device_count;

    return PCIE_OK;
}

static int pcie_scan_device(struct pcie_bus *bus, uint8_t bus_number, uint8_t device_number) {
    struct pcie_bdf bdf;

    uint32_t id;
    uint32_t header;

    uint16_t vendor;

    unsigned int function;
    int result;

    bdf.bus = bus_number;
    bdf.device = device_number;
    bdf.function = 0U;

    result = pcie_read32(bus, bdf, PCIE_CFG_VENDOR_DEVICE, &id);

    if (result != PCIE_OK)
	return result;

    vendor = (uint16_t) (id & 0xFFFFU);

    if (vendor == 0xFFFFU)
	return PCIE_OK;

    result = pcie_scan_function(bus, bus_number, device_number, 0U);

    if (result != PCIE_OK)
	return result;

    result = pcie_read32(bus, bdf, PCIE_CFG_HEADER, &header);

    if (result != PCIE_OK)
	return result;

    if ((header & 0x00800000U) == 0U)
	return PCIE_OK;

    for (function = 1U; function < 8U; ++function) {
        result = pcie_scan_function(bus, bus_number, device_number, (uint8_t)function);

	if (result != PCIE_OK)
            return result;
    }

    return PCIE_OK;
}

static int pcie_scan_bus(struct pcie_bus *bus, uint8_t bus_number) {
    int result;

    for (unsigned int device = 0U; device < 32U; ++device) {
        result = pcie_scan_device(bus, bus_number, (uint8_t) device);

	if (result != PCIE_OK)
            return result;
    }
    return PCIE_OK;
}

int pcie_bus_init(struct pcie_bus *bus, struct pcie_host *host) {
    if ((bus == NULL) || (host == NULL)) {
        return PCIE_ERR_ARGUMENT;
    }

    bus->host = host;
    bus->device_count = 0U;

    return PCIE_OK;
}

int pcie_enumerate(struct pcie_bus *bus) {
    if ((bus == NULL) || (bus->host == NULL)) {
        return PCIE_ERR_ARGUMENT;
    }

    bus->device_count = 0U;

    return pcie_scan_bus(bus, 0U);
}
