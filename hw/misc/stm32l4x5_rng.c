/*
 * STM32L4X5 RNG (Random Number Generator)
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
#include "hw/misc/stm32l4x5_rng.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/guest-random.h"

#ifndef STM_RNG_ERR_DEBUG
#define STM_RNG_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (STM_RNG_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static void stm32l4x5_rng_reset(DeviceState *dev)
{
    STM32L4X5RngState *s = STM32L4X5_RNG(dev);

    s->rng_cr = 0x00000000;
    s->rng_dr = 0x00000000;

    s->data_ready = false;
}

static uint64_t stm32l4x5_rng_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    STM32L4X5RngState *s = opaque;

    DB_PRINT("0x%"HWADDR_PRIx"\n", addr);

    switch (addr) {
    case RNG_CR:
        return s->rng_cr;
    case RNG_DR:
        // TODO: replace data
        return s->rng_dr;
    case RNG_SR:
        if (s->data_ready) {
            DB_PRINT("data is ready\n");
            return 0x1;
        } else {
            return 0x0;
        }
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
        return 0;
    }

    return 0;
}

static void stm32l4x5_rng_write(void *opaque, hwaddr addr,
                       uint64_t val64, unsigned int size)
{
    STM32L4X5RngState *s = opaque;
    uint32_t value = val64;

    DB_PRINT("0x%x, 0x%"HWADDR_PRIx"\n", value, addr);

    switch (addr) {
    case RNG_CR:
        if (value & 0x4) { // RNGEN
            s->data_ready = true;
            qemu_guest_getrandom_nofail(&s->rng_dr, 4);
        }
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
    }
}

static const MemoryRegionOps stm32l4x5_rng_ops = {
    .read = stm32l4x5_rng_read,
    .write = stm32l4x5_rng_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32l4x5_rng_init(Object *obj)
{
    STM32L4X5RngState *s = STM32L4X5_RNG(obj);

    memory_region_init_io(&s->mmio, obj, &stm32l4x5_rng_ops, s,
                          TYPE_STM32L4X5_RNG, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void stm32l4x5_rng_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32l4x5_rng_reset;
}

static const TypeInfo stm32l4x5_rng_info = {
    .name          = TYPE_STM32L4X5_RNG,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32L4X5RngState),
    .instance_init = stm32l4x5_rng_init,
    .class_init    = stm32l4x5_rng_class_init,
};

static void stm32l4x5_rng_register_types(void)
{
    type_register_static(&stm32l4x5_rng_info);
}

type_init(stm32l4x5_rng_register_types)
