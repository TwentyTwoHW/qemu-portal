/*
 * STM32L4X5 PWR (Real time clock)
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
#include "hw/misc/stm32l4x5_pwr.h"
#include "qemu/log.h"
#include "qemu/module.h"

#ifndef STM_PWR_ERR_DEBUG
#define STM_PWR_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (STM_PWR_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static void stm32l4x5_pwr_reset(DeviceState *dev)
{
    STM32L4X5PwrState *s = STM32L4X5_PWR(dev);

    s->pwr_cr1 = 0x00000000;
}

static uint64_t stm32l4x5_pwr_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    STM32L4X5PwrState *s = opaque;

    DB_PRINT("0x%"HWADDR_PRIx"\n", addr);

    switch (addr) {
    case PWR_CR1:
        return s->pwr_cr1;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
        return 0;
    }

    return 0;
}

static void stm32l4x5_pwr_write(void *opaque, hwaddr addr,
                       uint64_t val64, unsigned int size)
{
    STM32L4X5PwrState *s = opaque;
    uint32_t value = val64;

    DB_PRINT("0x%x, 0x%"HWADDR_PRIx"\n", value, addr);

    switch (addr) {
    case PWR_CR1:
        s->pwr_cr1 = (value & 0xFFFF);
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
    }
}

static const MemoryRegionOps stm32l4x5_pwr_ops = {
    .read = stm32l4x5_pwr_read,
    .write = stm32l4x5_pwr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32l4x5_pwr_init(Object *obj)
{
    STM32L4X5PwrState *s = STM32L4X5_PWR(obj);

    memory_region_init_io(&s->mmio, obj, &stm32l4x5_pwr_ops, s,
                          TYPE_STM32L4X5_PWR, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void stm32l4x5_pwr_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32l4x5_pwr_reset;
}

static const TypeInfo stm32l4x5_pwr_info = {
    .name          = TYPE_STM32L4X5_PWR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32L4X5PwrState),
    .instance_init = stm32l4x5_pwr_init,
    .class_init    = stm32l4x5_pwr_class_init,
};

static void stm32l4x5_pwr_register_types(void)
{
    type_register_static(&stm32l4x5_pwr_info);
}

type_init(stm32l4x5_pwr_register_types)
