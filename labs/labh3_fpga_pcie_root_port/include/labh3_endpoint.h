#ifndef LABH3_ENDPOINT_H
#define LABH3_ENDPOINT_H

#include <stdint.h>

/* H3 lab topology */
#ifndef LABH3_ENDPOINT_BUS
#define LABH3_ENDPOINT_BUS      0U
#endif

#ifndef LABH3_ENDPOINT_DEV
#define LABH3_ENDPOINT_DEV      1U
#endif

#ifndef LABH3_ENDPOINT_FN       
#define LABH3_ENDPOINT_FN       0U
#endif

/*
 * This is not a standard PCIe register
 */
#define LABH3_BAR0_SCRATCH_OFFSET  0x00001000UL

#define LABH3_TEST_VALUE           0xA5A55A5AU

#endif
