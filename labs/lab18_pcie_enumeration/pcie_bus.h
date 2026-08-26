#ifndef PCIE_BUS_H
#define PCIE_BUS_H

#include <stddef.h>
#include <stdint.h>

#include "pcie_host.h"

#define PCIE_MAX_DEVICES           16U
#define PCIE_MAX_BARS              6U

#define PCIE_HEADER_TYPE_NORMAL   0x00U
#define PCIE_HEADER_TYPE_BRIDGE   0x01U
#define PCIE_HEAERR_TYPE_MULTI    0x80U

struct pcie_bar {
    uint32_t raw;
    uint32_t address;

    uint8_t is_io;
    uint8_t is_64bit;
    uint8_t prefetchable;
};

struct pcie_device {
    struct pcie_bdf bdf;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t revision;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;

    uint8_t header_type;
    uint8_t multifunction;

    struct pcie_bar bars[PCIE_MAX_BARS];

    uint8_t irq_line;
    uint8_t irq_pin;
};

struct pcie_bus {
    struct pcie_host *host;

    struct pcie_device devices[PCIE_MAX_DEVICES];

    size_t device_count;
};

int pcie_bus_init(struct pcie_bus *bus, struct pcie_host *host);

int pcie_enumerate(struct pcie_bus *bus);

#endif
