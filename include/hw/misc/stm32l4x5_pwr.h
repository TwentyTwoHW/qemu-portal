/*
 * STM32L4X5 PWR (Power Control)
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

#ifndef HW_STM32L4X5_PWR_H
#define HW_STM32L4X5_PWR_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define PWR_CR1        0x00


#define TYPE_STM32L4X5_PWR "stm32l4x5-pwr"
OBJECT_DECLARE_SIMPLE_TYPE(STM32L4X5PwrState, STM32L4X5_PWR)

struct STM32L4X5PwrState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t pwr_cr1;
};

#endif /* HW_STM32L4X5_PWR_H */
