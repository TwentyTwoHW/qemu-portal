/*
 * STM32L4X5 TSC (Touch Sense Controller)
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
#include "hw/misc/stm32l4x5_tsc.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/irq.h"
#include "qemu/log.h"
#include "qemu/module.h"

#ifndef STM_TSC_ERR_DEBUG
#define STM_TSC_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (STM_TSC_ERR_DEBUG >= lvl) { \
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
    STM32L4X5TscState *s = opaque;

    DB_PRINT("Receiving byte %02x\n", buf[0]);

    if (buf[0] == 0x0a) {
        return;
    }

    s->value = buf[0] * 20;

    if (s->tsc_ier > 0) {
        DB_PRINT("triggering irq\n");
        qemu_set_irq(s->irq, 1);
    }
}

static void stm32l4x5_tsc_reset(DeviceState *dev)
{
    STM32L4X5TscState *s = STM32L4X5_TSC(dev);

    s->tsc_ier = 0;
    s->enabled = false;

    s->value = 0;

    qemu_chr_fe_set_handlers(&s->chr, stm32l4x5_usart_can_receive,
                             stm32l4x5_usart_receive, NULL, NULL,
                             s, NULL, true);
}

static uint64_t stm32l4x5_tsc_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    STM32L4X5TscState *s = opaque;

    DB_PRINT("0x%"HWADDR_PRIx"\n", addr);

    switch (addr) {
    case TSC_IOG2CR:
        DB_PRINT("returning read value %d\n", s->value);
        return s->value;
    default:
        // qemu_log_mask(LOG_GUEST_ERROR,
        //               "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
        return 0;
    }

    return 0;
}

static void stm32l4x5_tsc_write(void *opaque, hwaddr addr,
                       uint64_t val64, unsigned int size)
{
    STM32L4X5TscState *s = opaque;
    uint32_t value = val64;

    DB_PRINT("0x%x, 0x%"HWADDR_PRIx"\n", value, addr);

    switch (addr) {
    case TSC_CR:
        s->enabled = value & TSC_CR_TSCE;
        if (value & TSC_CR_START) {
            uint8_t v = 'S';
            qemu_chr_fe_write_all(&s->chr, &v, 1);
        }
        return;
    case TSC_IER:
        s->tsc_ier = value;
        return;
    case TSC_ICR:
        if (value > 0) {
            qemu_set_irq(s->irq, 0);
        }
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
    }
}

static const MemoryRegionOps stm32l4x5_tsc_ops = {
    .read = stm32l4x5_tsc_read,
    .write = stm32l4x5_tsc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static Property stm32l4x5_usart_properties[] = {
    DEFINE_PROP_CHR("chardev", STM32L4X5TscState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32l4x5_tsc_init(Object *obj)
{
    STM32L4X5TscState *s = STM32L4X5_TSC(obj);

    DB_PRINT("init\n");

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->mmio, obj, &stm32l4x5_tsc_ops, s,
                          TYPE_STM32L4X5_TSC, 0x400);

    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void stm32l4x5_tsc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32l4x5_tsc_reset;
    device_class_set_props(dc, stm32l4x5_usart_properties);
}

static const TypeInfo stm32l4x5_tsc_info = {
    .name          = TYPE_STM32L4X5_TSC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32L4X5TscState),
    .instance_init = stm32l4x5_tsc_init,
    .class_init    = stm32l4x5_tsc_class_init,
};

static void stm32l4x5_tsc_register_types(void)
{
    type_register_static(&stm32l4x5_tsc_info);
}

type_init(stm32l4x5_tsc_register_types)
