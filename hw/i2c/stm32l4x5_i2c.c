/*
 * STM32F405 I2C
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

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/i2c/stm32l4x5_i2c.h"
#include "migration/vmstate.h"

#ifndef STM_I2C_ERR_DEBUG
#define STM_I2C_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (STM_I2C_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static int stm32l4x5_usart_can_receive(void *opaque)
{
    STM32L4X5I2CState *s = opaque;

    return (s->i2c_isr & STM_I2C_ISR_RXNE) == 0 && !s->writing && s->nbytes > 0;
}

static void stm32l4x5_usart_receive(void *opaque, const uint8_t *buf, int size)
{
    STM32L4X5I2CState *s = opaque;

    DB_PRINT("Receiving byte %02x\n", buf[0]);

    if (s->i2c_isr & STM_I2C_ISR_RXNE) {
        DB_PRINT("dropping bytes!!\n");
    }

    if (!s->writing && s->nbytes > 0) {
        s->i2c_isr |= STM_I2C_ISR_RXNE;
        s->i2c_rxdr = buf[0];
    }
}

static void stm32l4x5_i2c_reset(DeviceState *dev)
{
    STM32L4X5I2CState *s = STM32L4X5_I2C(dev);

    s->i2c_cr1 = 0x00000000;
    s->i2c_cr2 = 0x00000000;
    s->i2c_isr = 0x00000001;

    s->enabled = false;
    s->writing = true;
    s->nbytes = 0;

    s->buf_index = 0;

    qemu_chr_fe_set_handlers(&s->chr, stm32l4x5_usart_can_receive,
                             stm32l4x5_usart_receive, NULL, NULL,
                             s, NULL, true);
}

static void stm32l4x5_i2c_transfer(STM32L4X5I2CState *s)
{
    i2c_start_send(s->i2c, s->i2c_txdr);

    s->buf[s->buf_index++] = s->i2c_txdr;
    if (s->buf_index == BUF_SIZE) {
        qemu_chr_fe_write_all(&s->chr, s->buf, s->buf_index);
        s->buf_index = 0;
    }

    // s->i2c_dr = i2c_transfer(s->i2c, s->i2c_dr);
    s->i2c_isr |= (0b11);
}

static uint64_t stm32l4x5_i2c_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    STM32L4X5I2CState *s = opaque;

    // DB_PRINT("Address: 0x%" HWADDR_PRIx "\n", addr);

    switch (addr) {
    case STM_I2C_CR1:
        return s->i2c_cr1;
    case STM_I2C_CR2:
        // qemu_log_mask(LOG_UNIMP, "%s: Interrupts and DMA are not implemented\n",
        //               __func__);
        return s->i2c_cr2;
    case STM_I2C_ISR:
        // DB_PRINT("isr value = %08x\n", s->i2c_isr);
        return s->i2c_isr;
    case STM_I2C_RXDR:
        s->i2c_isr &= ~STM_I2C_ISR_RXNE;
        return s->i2c_rxdr;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
    }

    return 0;
}

static void stm32l4x5_i2c_write(void *opaque, hwaddr addr,
                                uint64_t val64, unsigned int size)
{
    STM32L4X5I2CState *s = opaque;
    uint32_t value = val64;

    DB_PRINT("Address: 0x%" HWADDR_PRIx ", Value: 0x%x\n", addr, value);

    switch (addr) {
    case STM_I2C_CR1:
        s->i2c_cr1 = value;
        if (value & 1) {
            s->enabled = true;
            s->i2c_isr |= STM_I2C_ISR_TXIS;
            s->nbytes = 0;
        }
        return;
    case STM_I2C_CR2:
        s->i2c_cr2 = value & (~STM_I2C_CR2_START);
        s->nbytes = (value >> 16) & 0xFF;

        if (value & STM_I2C_CR2_RD_WRN) {
            DB_PRINT("reading %d bytes\n", s->nbytes);
            s->writing = false;
        } else {
            s->writing = true;
        }

        if (value & STM_I2C_CR2_STOP) {
            // Ignore this i guess?
            value &= ~STM_I2C_CR2_STOP;
        }

        if (s->nbytes > 0) {
            s->i2c_isr &= ~STM_I2C_ISR_TC;
        }
        return;
    case STM_I2C_TXDR:
        s->i2c_txdr = value & 0xFFFF;
        if (s->writing) {
            s->nbytes--;
        }

        stm32l4x5_i2c_transfer(s);

        if (s->nbytes == 0) {
            s->i2c_isr |= STM_I2C_ISR_TC;
            qemu_chr_fe_write_all(&s->chr, s->buf, s->buf_index);
            s->buf_index = 0;
        }
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%" HWADDR_PRIx "\n", __func__, addr);
    }
}

static const MemoryRegionOps stm32l4x5_i2c_ops = {
    .read = stm32l4x5_i2c_read,
    .write = stm32l4x5_i2c_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_stm32l4x5_i2c = {
    .name = TYPE_STM32L4X5_I2C,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(i2c_cr1, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_cr2, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_oar1, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_oar2, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_timingr, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_isr, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_icr, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_pecr, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_rxdr, STM32L4X5I2CState),
        VMSTATE_UINT32(i2c_txdr, STM32L4X5I2CState),
        VMSTATE_END_OF_LIST()
    }
};

static void stm32l4x5_i2c_init(Object *obj)
{
    STM32L4X5I2CState *s = STM32L4X5_I2C(obj);
    DeviceState *dev = DEVICE(obj);

    memory_region_init_io(&s->mmio, obj, &stm32l4x5_i2c_ops, s,
                          TYPE_STM32L4X5_I2C, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    s->i2c = i2c_init_bus(dev, "i2c");
}

static Property stm32l4x5_usart_properties[] = {
    DEFINE_PROP_CHR("chardev", STM32L4X5I2CState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32l4x5_i2c_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32l4x5_i2c_reset;
    device_class_set_props(dc, stm32l4x5_usart_properties);
    dc->vmsd = &vmstate_stm32l4x5_i2c;
}

static const TypeInfo stm32l4x5_i2c_info = {
    .name          = TYPE_STM32L4X5_I2C,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32L4X5I2CState),
    .instance_init = stm32l4x5_i2c_init,
    .class_init    = stm32l4x5_i2c_class_init,
};

static void stm32l4x5_i2c_register_types(void)
{
    type_register_static(&stm32l4x5_i2c_info);
}

type_init(stm32l4x5_i2c_register_types)
