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

#ifndef HW_STM32L4X5_RTC_H
#define HW_STM32L4X5_RTC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define RTC_TR         0x00
#define RTC_DR         0x04
#define RTC_CR         0x08
#define RTC_ISR        0x0C
#define RTC_WPR        0x24

#define RTC_ISR_INIT    0x80
#define RTC_ISR_INITF   0x40

#define RTC_BKPR_REG_NUM 32


#define TYPE_STM32L4X5_RTC "stm32l4x5-rtc"
OBJECT_DECLARE_SIMPLE_TYPE(STM32L4X5RtcState, STM32L4X5_RTC)

struct STM32L4X5RtcState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t write_protected_state;

    uint32_t rtc_tr;
    uint32_t rtc_dr;
    uint32_t rtc_cr;
    uint32_t rtc_isr;

    uint32_t rtc_bkpr[RTC_BKPR_REG_NUM];
};

#endif /* HW_STM32L4X5_RTC_H */
