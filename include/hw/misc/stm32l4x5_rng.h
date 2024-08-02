/*
 * STM32L4X5 RNG (Power Control)
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

#ifndef HW_STM32L4X5_RNG_H
#define HW_STM32L4X5_RNG_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define RNG_CR        0x00
#define RNG_SR        0x04
#define RNG_DR        0x08


#define TYPE_STM32L4X5_RNG "stm32l4x5-rng"
OBJECT_DECLARE_SIMPLE_TYPE(STM32L4X5RngState, STM32L4X5_RNG)

struct STM32L4X5RngState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    bool data_ready;

    uint32_t rng_cr;
    uint32_t rng_dr;
};

#endif /* HW_STM32L4X5_RNG_H */
