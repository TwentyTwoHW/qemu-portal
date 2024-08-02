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

#include "qemu/osdep.h"
#include "hw/misc/stm32l4x5_gpio_int.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/irq.h"
#include "qemu/log.h"
#include "qemu/module.h"

#ifndef STM_GPIO_INT_ERR_DEBUG
#define STM_GPIO_INT_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (STM_GPIO_INT_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static int stm32l4x5_usart_can_receive(void *opaque)
{
    return 1;
}

static void stm32l4x5_usart_receive(void *opaque, const uint8_t *buf, int size)
{
    STM32L4X5GpioIntState *s = opaque;

    DB_PRINT("triggering irq\n");
    qemu_set_irq(s->irq, 0);
}

static void stm32l4x5_gpio_int_reset(DeviceState *dev)
{
    STM32L4X5GpioIntState *s = STM32L4X5_GPIO_INT(dev);

    qemu_chr_fe_set_handlers(&s->chr, stm32l4x5_usart_can_receive,
                             stm32l4x5_usart_receive, NULL, NULL,
                             s, NULL, true);
}

static Property stm32l4x5_usart_properties[] = {
    DEFINE_PROP_CHR("chardev", STM32L4X5GpioIntState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32l4x5_gpio_int_init(Object *obj)
{
    STM32L4X5GpioIntState *s = STM32L4X5_GPIO_INT(obj);

    DB_PRINT("init\n");

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
}

static void stm32l4x5_gpio_int_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32l4x5_gpio_int_reset;
    device_class_set_props(dc, stm32l4x5_usart_properties);
}

static const TypeInfo stm32l4x5_gpio_int_info = {
    .name          = TYPE_STM32L4X5_GPIO_INT,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32L4X5GpioIntState),
    .instance_init = stm32l4x5_gpio_int_init,
    .class_init    = stm32l4x5_gpio_int_class_init,
};

static void stm32l4x5_gpio_int_register_types(void)
{
    type_register_static(&stm32l4x5_gpio_int_info);
}

type_init(stm32l4x5_gpio_int_register_types)
