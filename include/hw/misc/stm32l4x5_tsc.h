/*
 * STM32L4X5 TSC (Touch sense controller)
 *
 * Copyright (c) 2024 Alekos Filini <alekos.filini@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * The reference used is the STMicroElectronics RM0351 Reference manual
 * for STM32L4x5 and STM32L4x6 advanced Arm ® -based 32-bit MCUs.
 *
 * Inspired by the STM32Lx5 RCC
 */

#ifndef HW_STM32L4X5_TSC_H
#define HW_STM32L4X5_TSC_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "chardev/char-fe.h"

#define TSC_CR        0x00
#define TSC_IER        0x04
#define TSC_ICR        0x08
#define TSC_ISR        0x0C
#define TSC_IOCCR     0x28
#define TSC_IOG2CR     0x38

#define TSC_CR_TSCE 0x1
#define TSC_CR_START 0x2

#define TSC_IER_EOAIC 0x1
#define TSC_IER_MCEIC 0x2


#define TYPE_STM32L4X5_TSC "stm32l4x5-tsc"
OBJECT_DECLARE_SIMPLE_TYPE(STM32L4X5TscState, STM32L4X5_TSC)

struct STM32L4X5TscState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    bool enabled;
    uint32_t tsc_ier;

    uint16_t value;

    CharBackend chr;

    qemu_irq irq;
};

#endif /* HW_STM32L4X5_TSC_H */
