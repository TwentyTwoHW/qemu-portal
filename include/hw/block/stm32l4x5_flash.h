/*
 * STM32L4X5 FLASH
 *
 * Copyright (c) 2024 Alekos Filini <alekos.filini@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef HW_STM32L4X5_FLASH_H
#define HW_STM32L4X5_FLASH_H

#define TYPE_STM32L4X5_FLASH "stm32l4x5-flash"
OBJECT_DECLARE_SIMPLE_TYPE(STM32L4X5FlashState, STM32L4X5_FLASH)
 
#define FLASH_ACR      0x0
#define FLASH_KEYR     0x8
#define FLASH_SR       0x10
#define FLASH_CR       0x14

#define FLASH_SR_EOP        (1 << 0)
#define FLASH_SR_PROGERR    (1 << 3)
#define FLASH_SR_BSY        (1 << 16)

#define FLASH_CR_PG         (1 << 0)
#define FLASH_CR_PER        (1 << 1)
#define FLASH_CR_MER1       (1 << 2)
#define FLASH_CR_BKER       (1 << 11)
#define FLASH_CR_MER2       (1 << 15)
#define FLASH_CR_STRT       (1 << 16)
#define FLASH_CR_LOCK       (1 << 31)

#define NUM_BANKS 2
#define PAGE_SIZE 2048
#define NUM_PAGES_BANK 256
#define BANK_SIZE (NUM_PAGES_BANK * PAGE_SIZE)

struct STM32L4X5FlashState {
    SysBusDevice parent_obj;

    BlockBackend *blk;
    MemoryRegion mmio;

    MemoryRegion container;
    MemoryRegion bank[NUM_BANKS];

    uint32_t *content[NUM_BANKS];

    uint32_t flash_cr;
    uint32_t flash_sr;

    uint32_t prev_write_addr;

    uint8_t unlock_stage;
    bool swapped_banks;
};

#endif /* HW_STM32L4X5_FLASH_H */