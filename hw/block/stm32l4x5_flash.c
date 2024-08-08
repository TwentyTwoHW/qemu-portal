/*
 * STM32L4X5 Flash
 *
 * Copyright (c) 2024 Alekos Filini <alekos.filini@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/block/flash.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "sysemu/block-backend.h"
#include "exec/memory.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "hw/block/stm32l4x5_flash.h"

#ifndef STM_FLASH_ERR_DEBUG
#define STM_FLASH_ERR_DEBUG 0
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (STM_FLASH_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

static void stm32l4x5_flash_system_reset(DeviceState *dev)
{
    STM32L4X5FlashState *s = STM32L4X5_FLASH(dev);

    s->unlock_stage = 0;

    s->flash_sr = 0;
    s->flash_cr = 0xC0000000;

    s->prev_write_addr = 0;
}

static uint64_t stm32l4x5_flash_mmio_read(void *opaque, hwaddr addr,
                             unsigned size)
{
    STM32L4X5FlashState *s = (STM32L4X5FlashState *) opaque;
    DB_PRINT("Read reg %08lx\n", addr);

    switch (addr) {
    case FLASH_SR:
        return s->flash_sr;
    case FLASH_CR:
        return s->flash_cr;
    }

    return 0;
}

static void stm32l4x5_flash_mmio_write(void *opaque, hwaddr addr,
                          uint64_t value, unsigned size)
{
    STM32L4X5FlashState *s = (STM32L4X5FlashState *) opaque;

    uint32_t val = value;
    DB_PRINT("Write reg %08lx = %08x\n", addr, val);

    switch (addr) {
    case FLASH_SR:
        s->flash_sr &= ~val;
        if (val & FLASH_SR_BSY) {
            s->flash_cr &= ~FLASH_CR_STRT;
        }
        return;
    case FLASH_CR:
        if (val & FLASH_CR_LOCK) {
            s->flash_cr |= FLASH_CR_LOCK;
            s->unlock_stage = 0;
            return;
        }

        s->flash_cr = val & ~FLASH_CR_LOCK;

        if (val & FLASH_CR_STRT) {
            if (s->flash_cr & FLASH_CR_LOCK) {
                s->flash_sr = FLASH_SR_PROGERR;
                return;
            }

            if (val & FLASH_CR_PG) {
                DB_PRINT("Programming\n");
                s->flash_sr |= FLASH_SR_BSY;
                return;
            }

            if (val & FLASH_CR_PER) {
                uint32_t page = (val >> 3) & 0xFF;
                size_t bank = (value & FLASH_CR_BKER) > 0;
                DB_PRINT("Erasing page %x in bank %lx\n", page, bank);
                memset(&s->content[bank][(page << 9)], 0xFF, PAGE_SIZE);
                DB_PRINT("Offset %x\n", (page << bank) << 10);
                if (blk_pwrite(s->blk, (page << bank) << 10, PAGE_SIZE, &s->content[bank][page << 8], 0) < 0) {
                    DB_PRINT("error writing to disk\n");
                }
            }
            if (val & FLASH_CR_MER1) {
                DB_PRINT("Mass erase 1\n");
                memset(&s->content[0], 0xFF, BANK_SIZE);
                if (blk_pwrite(s->blk, 0, BANK_SIZE, s->content[0], 0) < 0) {
                    DB_PRINT("error writing to disk\n");
                }
            }
            if (val & FLASH_CR_MER2) {
                DB_PRINT("Mass erase 2\n");
                memset(&s->content[1], 0xFF, BANK_SIZE);
                if (blk_pwrite(s->blk, BANK_SIZE, BANK_SIZE, &s->content[1], 0) < 0) {
                    DB_PRINT("error writing to disk\n");
                }
            }

            s->flash_sr |= FLASH_SR_EOP;
        }
        return;
    case FLASH_KEYR:
        if (s->unlock_stage == 0 && val == 0x45670123) {
            s->unlock_stage = 1;
        } else if (s->unlock_stage == 1 && val == 0xCDEF89AB) {
            s->unlock_stage = 0;
            s->flash_cr &= ~FLASH_CR_LOCK;
        } else {
            s->unlock_stage = 0;
        }
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "write to unknown STM32L4X5Flash register 0x%lx\n",
                      addr);
    }
}

static const MemoryRegionOps stm32l4x5_flash_mmio_ops = {
    .read = stm32l4x5_flash_mmio_read,
    .write = stm32l4x5_flash_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

// static MemTxResult stm32l4x5_flash_read(void *opaque, hwaddr addr, uint64_t *value,
//                              unsigned size, MemTxAttrs attrs)
// {
//     STM32L4X5FlashState *s = (STM32L4X5FlashState *) opaque;
//     DB_PRINT("Read flash addr %08lx\n", addr);
// 
//     uint32_t effective_addr = addr >> 2;
//     if (s->swapped_banks) {
//         effective_addr ^= 0x20000;
//     }
// 
//     *value = s->content[effective_addr];
//     return MEMTX_OK;
// }

static MemTxResult stm32l4x5_flash_write(void *opaque, hwaddr addr,
                          uint64_t value, unsigned size, MemTxAttrs attrs)
{
    STM32L4X5FlashState *s = (STM32L4X5FlashState *) opaque;

    uint32_t val = value;
    DB_PRINT("orig addr = %lx", addr);
    uint32_t effective_addr = addr >> 2;
    size_t bank = 0;
    if (effective_addr & 0x20000) {
        effective_addr &= 0x1FFFF;
        bank = 1;
    }
    if (s->swapped_banks) {
        bank = !bank;
    }

    // DB_PRINT("Write flash addr %08x = %08x (prev = %08x)\n", effective_addr, val, s->prev_write_addr);

    if (s->flash_cr & FLASH_CR_LOCK
        || addr & 0b11
        || !(s->flash_cr & FLASH_CR_PG)
        || (s->prev_write_addr > 0 && s->prev_write_addr + 1 != effective_addr)
        || (s->content[bank][effective_addr] != 0x00 && s->content[bank][effective_addr] != 0xFFFFFFFF)) {

        s->flash_sr &= ~FLASH_SR_BSY;
        s->flash_sr |= FLASH_SR_PROGERR;
        return MEMTX_OK;
    }

    if (s->prev_write_addr == 0 && effective_addr != 1) {
        s->prev_write_addr = effective_addr;
    } else {
        s->prev_write_addr = 0;
        s->flash_sr &= ~FLASH_SR_BSY;
        s->flash_sr |= FLASH_SR_EOP;
    }

    s->content[bank][effective_addr] = val;
    if (bank) {
        effective_addr |= 0x20000;
    }
    if (blk_pwrite(s->blk, effective_addr << 2, 4, &val, 0) < 0) {
        DB_PRINT("error writing to disk\n");
    }

    return MEMTX_OK;
}

static const MemoryRegionOps stm32l4x5_flash_ops = {
    // .read_with_attrs = stm32l4x5_flash_read,
    .write_with_attrs = stm32l4x5_flash_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32l4x5_flash_mem_setup(STM32L4X5FlashState *s)
{
}

static void stm32l4x5_flash_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    STM32L4X5FlashState *s = STM32L4X5_FLASH(dev);
    Error *local_err = NULL;

    if (!s->blk) {
        error_setg(errp, "blk not set");
        return;
    }
    if (!blk_supports_write_perm(s->blk)) {
        error_setg(errp, "Can't use a read-only drive");
        return;
    }
    blk_set_perm(s->blk, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
                 BLK_PERM_ALL, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    memory_region_init_io(&s->mmio, OBJECT(s), &stm32l4x5_flash_mmio_ops, s,
                          "stm32l4x5-flash-mmio", 0x400);
    sysbus_init_mmio(sbd, &s->mmio);

    for (size_t i = 0; i < NUM_BANKS; i++) {
        char name[30];
        snprintf(name, 30, "stm32l4x5-flash-eeprom[%lu]", i);
        memory_region_init_rom_device(&s->bank[i], OBJECT(s), &stm32l4x5_flash_ops, s,
                              name, BANK_SIZE, errp);
        sysbus_init_mmio(sbd, &s->bank[i]);
        s->content[i] = memory_region_get_ram_ptr(&s->bank[i]);

        if (blk_pread(s->blk, i * BANK_SIZE, BANK_SIZE, s->content[i], 0) < 0) {
            error_setg(errp, "Failed to load flash content");
            return;
        }
    }

    memory_region_init(&s->container, OBJECT(s), TYPE_STM32L4X5_FLASH, NUM_BANKS * PAGE_SIZE * NUM_PAGES_BANK);
    sysbus_init_mmio(sbd, &s->container);

    // Implement the bank switching logic here
    uint32_t first_addr = __bswap_32 (s->content[1][0]);
    if (first_addr >= 0x0800000 && first_addr < 0x0800000 + BANK_SIZE) {
        DB_PRINT("Detected fw in bank2\n");
        s->swapped_banks = true;
    }

    size_t banks[NUM_BANKS] = { 0, 1 };
    if (s->swapped_banks) {
        banks[0] = 1;
        banks[1] = 0;
    }
    for (size_t i = 0; i < NUM_BANKS; i++) {
        memory_region_add_subregion(&s->container, i * BANK_SIZE, &s->bank[banks[i]]);
    }

    // memory_region_init_io(&s->flash, OBJECT(s), &stm32l4x5_flash_ops, s,
    //                       TYPE_STM32L4X5_FLASH, NUM_BANKS * PAGE_SIZE * NUM_PAGES_BANK);
    // sysbus_init_mmio(sbd, &s->flash);
    // vmstate_register(VMSTATE_IF(dev),
    //                  ((s->shift & 0x7f) << 24)
    //                  | ((s->id.man & 0xff) << 16)
    //                  | ((s->id.dev & 0xff) << 8)
    //                  | (s->id.ver & 0xff),
    //                  &vmstate_stm32l4x5_flash, s);
}

static Property stm32l4x5_flash_properties[] = {
    DEFINE_PROP_DRIVE("drive", STM32L4X5FlashState, blk),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32l4x5_flash_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32l4x5_flash_realize;
    dc->reset = stm32l4x5_flash_system_reset;
    device_class_set_props(dc, stm32l4x5_flash_properties);
}

static const TypeInfo stm32l4x5_flash_info = {
    .name          = TYPE_STM32L4X5_FLASH,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32L4X5FlashState),
    .class_init    = stm32l4x5_flash_class_init,
};

static void stm32l4x5_flash_register_types(void)
{
    type_register_static(&stm32l4x5_flash_info);
}

type_init(stm32l4x5_flash_register_types)
