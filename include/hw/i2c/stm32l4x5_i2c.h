/*
 * STM32L4X5 I2C
 *
 * Copyright (c) 2024 Alekos Filini <alekos.filini@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef HW_STM32L4X5_I2C_H
#define HW_STM32L4X5_I2C_H

#include "hw/sysbus.h"
#include "hw/i2c/i2c.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define STM_I2C_CR1     0x00
#define STM_I2C_CR2     0x04
#define STM_I2C_OAR1      0x08
#define STM_I2C_OAR2      0x0C
#define STM_I2C_TIMINGR   0x10
#define STM_I2C_TIMEOUTR  0x14
#define STM_I2C_ISR  0x18
#define STM_I2C_ICR 0x1C
#define STM_I2C_PECR   0x20
#define STM_I2C_RXDR   0x24
#define STM_I2C_TXDR   0x28

#define STM_I2C_CR1_MSTR (1 << 2)
#define STM_I2C_CR1_SPE  (1 << 6)

#define STM_I2C_CR2_RD_WRN  (1 << 10)
#define STM_I2C_CR2_START   (1 << 13)
#define STM_I2C_CR2_STOP    (1 << 14)

#define STM_I2C_ISR_TXE    (1 << 0)
#define STM_I2C_ISR_TXIS   (1 << 1)
#define STM_I2C_ISR_RXNE   (1 << 2)
#define STM_I2C_ISR_TC     (1 << 6)

#define BUF_SIZE            64

#define TYPE_STM32L4X5_I2C "stm32f2xx-i2c"
OBJECT_DECLARE_SIMPLE_TYPE(STM32L4X5I2CState, STM32L4X5_I2C)

struct STM32L4X5I2CState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t i2c_cr1;
    uint32_t i2c_cr2;
    uint32_t i2c_oar1;
    uint32_t i2c_oar2;
    uint32_t i2c_timingr;
    uint32_t i2c_isr;
    uint32_t i2c_icr;
    uint32_t i2c_pecr;
    uint32_t i2c_rxdr;
    uint32_t i2c_txdr;

    bool enabled;
    uint8_t nbytes;

    bool writing;

    CharBackend chr;

    uint8_t buf[BUF_SIZE];
    size_t buf_index;
    size_t buf_len;

    qemu_irq irq;
    I2CBus *i2c;
};

#endif /* HW_STM32L4X5_I2C_H */
