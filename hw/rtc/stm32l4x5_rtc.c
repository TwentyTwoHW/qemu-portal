/*
 * STM32L4X5 RTC (Real time clock)
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
#include "hw/rtc/stm32l4x5_rtc.h"
#include "qemu/log.h"
#include "qemu/module.h"

#ifndef STM_RTC_ERR_DEBUG
#define STM_RTC_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (STM_RTC_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static void stm32l4x5_rtc_reset(DeviceState *dev)
{
    STM32L4X5RtcState *s = STM32L4X5_RTC(dev);

    s->rtc_tr = 0x00000000;
    s->rtc_dr = 0x00000000;
    s->rtc_cr = 0x00000000;
    s->rtc_isr = 0x00000007;

    s->write_protected_state = 0;
}

static uint64_t stm32l4x5_rtc_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    STM32L4X5RtcState *s = opaque;

    // DB_PRINT("0x%"HWADDR_PRIx"\n", addr);

    switch (addr) {
    case RTC_TR:
        return s->rtc_tr;
    case RTC_DR:
        return s->rtc_dr;
    case RTC_CR:
        return s->rtc_cr;
    case RTC_ISR:
        return s->rtc_isr;
    default:
        if (addr >= 0x50 && addr < 0x50 + RTC_BKPR_REG_NUM * 4) {
            return s->rtc_bkpr[(addr - 0x50) / 4];
        }

        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
        return 0;
    }

    return 0;
}

static void stm32l4x5_rtc_write(void *opaque, hwaddr addr,
                       uint64_t val64, unsigned int size)
{
    STM32L4X5RtcState *s = opaque;
    uint32_t value = val64;

    DB_PRINT("0x%x, 0x%"HWADDR_PRIx"\n", value, addr);

    if (addr >= 0x50 && addr < 0x50 + RTC_BKPR_REG_NUM * 4) {
        s->rtc_bkpr[(addr - 0x50) / 4] = value;
        return;
    }

    switch (addr) {
    case RTC_TR:
        // qemu_log_mask(LOG_UNIMP,
        //               "%s: Changing the memory mapping isn't supported in QEMU\n", __func__);
        s->rtc_tr = (value & 0xFFFF);
        return;
    case RTC_DR:
        s->rtc_dr = (value & 0xFFFF);
        return;
    case RTC_CR:
        s->rtc_cr = (value & 0xFFFF);
        return;
    case RTC_ISR:
        if (value & RTC_ISR_INIT) {
            s->rtc_isr |= RTC_ISR_INITF;
        } else {
            s->rtc_isr &= ~RTC_ISR_INITF;
        }
        return;
    case RTC_WPR:
        // TODO: currently unused
        if (s->write_protected_state == 0 && value == 0xCA) {
            s->write_protected_state = 1;
        } else if (s->write_protected_state == 1 && value == 0x53) {
            s->write_protected_state = 2;
        } else {
            s->write_protected_state = 0;
        }
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, addr);
    }
}

static const MemoryRegionOps stm32l4x5_rtc_ops = {
    .read = stm32l4x5_rtc_read,
    .write = stm32l4x5_rtc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32l4x5_rtc_init(Object *obj)
{
    STM32L4X5RtcState *s = STM32L4X5_RTC(obj);

    memory_region_init_io(&s->mmio, obj, &stm32l4x5_rtc_ops, s,
                          TYPE_STM32L4X5_RTC, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static void stm32l4x5_rtc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32l4x5_rtc_reset;
}

static const TypeInfo stm32l4x5_rtc_info = {
    .name          = TYPE_STM32L4X5_RTC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32L4X5RtcState),
    .instance_init = stm32l4x5_rtc_init,
    .class_init    = stm32l4x5_rtc_class_init,
};

static void stm32l4x5_rtc_register_types(void)
{
    type_register_static(&stm32l4x5_rtc_info);
}

type_init(stm32l4x5_rtc_register_types)
