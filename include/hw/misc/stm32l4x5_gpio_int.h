/*
 * STM32L4X5 GPIO_INT
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

#ifndef HW_STM32L4X5_GPIO_INT_H
#define HW_STM32L4X5_GPIO_INT_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "chardev/char-fe.h"

#define TYPE_STM32L4X5_GPIO_INT "stm32l4x5-gpio-int"
OBJECT_DECLARE_SIMPLE_TYPE(STM32L4X5GpioIntState, STM32L4X5_GPIO_INT)

struct STM32L4X5GpioIntState {
    /* <private> */
    SysBusDevice parent_obj;

    CharBackend chr;

    qemu_irq irq;
};

#endif /* HW_STM32L4X5_GPIO_INT_H */
