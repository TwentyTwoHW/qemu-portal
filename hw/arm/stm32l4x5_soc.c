/*
 * STM32L4x5 SoC family
 *
 * Copyright (c) 2023 Arnaud Minier <arnaud.minier@telecom-paris.fr>
 * Copyright (c) 2023 Inès Varhol <ines.varhol@telecom-paris.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * This work is heavily inspired by the stm32f405_soc by Alistair Francis.
 * Original code is licensed under the MIT License:
 *
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 */

/*
 * The reference used is the STMicroElectronics RM0351 Reference manual
 * for STM32L4x5 and STM32L4x6 advanced Arm ® -based 32-bit MCUs.
 * https://www.st.com/en/microcontrollers-microprocessors/stm32l4x5/documentation.html
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/or-irq.h"
#include "hw/arm/stm32l4x5_soc.h"
#include "hw/gpio/stm32l4x5_gpio.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"
#include "hw/block/flash.h"
#include "sysemu/blockdev.h"
#include "qemu/log.h"

#ifndef STM_SOC_ERR_DEBUG
#define STM_SOC_ERR_DEBUG 1
#endif

#define DB_PRINT_L(lvl, fmt, args...) do { \
    if (STM_SOC_ERR_DEBUG >= lvl) { \
        qemu_log("%s: " fmt, __func__, ## args); \
    } \
} while (0)

#define DB_PRINT(fmt, args...) DB_PRINT_L(1, fmt, ## args)

#define FLASH_BASE_ADDRESS 0x08000000
#define SRAM1_BASE_ADDRESS 0x20000000
#define SRAM1_SIZE (96 * KiB)
#define SRAM2_BASE_ADDRESS 0x10000000
#define SRAM2_SIZE (32 * KiB)

#define EXTI_ADDR 0x40010400
#define SYSCFG_ADDR 0x40010000

#define I2C_ADDR 0x40005400
#define TSC_ADDR 0x40024000

#define NUM_EXTI_IRQ 40
/* Match exti line connections with their CPU IRQ number */
/* See Vector Table (Reference Manual p.396) */
/*
 * Some IRQs are connected to the same CPU IRQ (denoted by -1)
 * and require an intermediary OR gate to function correctly.
 */
static const int exti_irq[NUM_EXTI_IRQ] = {
    6,                      /* GPIO[0]                 */
    7,                      /* GPIO[1]                 */
    8,                      /* GPIO[2]                 */
    9,                      /* GPIO[3]                 */
    10,                     /* GPIO[4]                 */
    -1, -1, -1, -1, -1,     /* GPIO[5..9] OR gate 23   */
    -1, -1, -1, -1, -1, -1, /* GPIO[10..15] OR gate 40 */
    -1,                     /* PVD OR gate 1           */
    67,                     /* OTG_FS_WKUP, Direct     */
    41,                     /* RTC_ALARM               */
    2,                      /* RTC_TAMP_STAMP2/CSS_LSE */
    3,                      /* RTC wakeup timer        */
    -1, -1,                 /* COMP[1..2] OR gate 63   */
    31,                     /* I2C1 wakeup, Direct     */
    33,                     /* I2C2 wakeup, Direct     */
    72,                     /* I2C3 wakeup, Direct     */
    37,                     /* USART1 wakeup, Direct   */
    38,                     /* USART2 wakeup, Direct   */
    39,                     /* USART3 wakeup, Direct   */
    52,                     /* UART4 wakeup, Direct    */
    53,                     /* UART4 wakeup, Direct    */
    70,                     /* LPUART1 wakeup, Direct  */
    65,                     /* LPTIM1, Direct          */
    66,                     /* LPTIM2, Direct          */
    76,                     /* SWPMI1 wakeup, Direct   */
    -1, -1, -1, -1,         /* PVM[1..4] OR gate 1     */
    78                      /* LCD wakeup, Direct      */
};

static const uint32_t i2c_addr[] =   { 0x40005400, 0x40005800 };
// static const int i2c_irq[] =   { 31, 32, 33, 34 };

#define RCC_BASE_ADDRESS 0x40021000
#define RCC_IRQ 5

#define RTC_BASE_ADDRESS 0x40002800
#define PWR_BASE_ADDRESS 0x40007000
#define RNG_BASE_ADDRESS 0x50060800
#define FLASH_MMIO_BASE_ADDRESS 0x40022000

static const int exti_or_gates_out[NUM_EXTI_OR_GATES] = {
    23, 40, 63, 1,
};

static const int exti_or_gates_num_lines_in[NUM_EXTI_OR_GATES] = {
    5, 6, 2, 5,
};

/* 3 OR gates with consecutive inputs */
#define NUM_EXTI_SIMPLE_OR_GATES 3
static const int exti_or_gates_first_line_in[NUM_EXTI_SIMPLE_OR_GATES] = {
    5, 10, 21,
};

/* 1 OR gate with non-consecutive inputs */
#define EXTI_OR_GATE1_NUM_LINES_IN 5
static const int exti_or_gate1_lines_in[EXTI_OR_GATE1_NUM_LINES_IN] = {
    16, 35, 36, 37, 38,
};

static const struct {
    uint32_t addr;
    uint32_t moder_reset;
    uint32_t ospeedr_reset;
    uint32_t pupdr_reset;
} stm32l4x5_gpio_cfg[NUM_GPIOS] = {
    { 0x48000000, 0xABFFFFFF, 0x0C000000, 0x64000000 },
    { 0x48000400, 0xFFFFFEBF, 0x00000000, 0x00000100 },
    { 0x48000800, 0xFFFFFFFF, 0x00000000, 0x00000000 },
    { 0x48000C00, 0xFFFFFFFF, 0x00000000, 0x00000000 },
    { 0x48001000, 0xFFFFFFFF, 0x00000000, 0x00000000 },
    { 0x48001400, 0xFFFFFFFF, 0x00000000, 0x00000000 },
    { 0x48001800, 0xFFFFFFFF, 0x00000000, 0x00000000 },
    { 0x48001C00, 0x0000000F, 0x00000000, 0x00000000 },
};

static void stm32l4x5_soc_initfn(Object *obj)
{
    Stm32l4x5SocState *s = STM32L4X5_SOC(obj);

    object_initialize_child(obj, "exti", &s->exti, TYPE_STM32L4X5_EXTI);
    for (unsigned i = 0; i < NUM_EXTI_OR_GATES; i++) {
        object_initialize_child(obj, "exti_or_gates[*]", &s->exti_or_gates[i],
                                TYPE_OR_IRQ);
    }
    object_initialize_child(obj, "syscfg", &s->syscfg, TYPE_STM32L4X5_SYSCFG);
    object_initialize_child(obj, "rcc", &s->rcc, TYPE_STM32L4X5_RCC);

    object_initialize_child(obj, "rtc", &s->rtc, TYPE_STM32L4X5_RTC);
    object_initialize_child(obj, "pwr", &s->pwr, TYPE_STM32L4X5_PWR);
    object_initialize_child(obj, "rng", &s->rng, TYPE_STM32L4X5_RNG);

    for (unsigned i = 0; i < NUM_I2C; i++) {
        object_initialize_child(obj, "i2c[*]", &s->i2c[i], TYPE_STM32L4X5_I2C);
    }

    object_initialize_child(obj, "tsc", &s->tsc, TYPE_STM32L4X5_TSC);
    object_initialize_child(obj, "gpio-int", &s->gpio_int, TYPE_STM32L4X5_GPIO_INT);

    for (unsigned i = 0; i < NUM_GPIOS; i++) {
        g_autofree char *name = g_strdup_printf("gpio%c", 'a' + i);
        object_initialize_child(obj, name, &s->gpio[i], TYPE_STM32L4X5_GPIO);
    }

    object_initialize_child(obj, "flash", &s->flash, TYPE_STM32L4X5_FLASH);
}

static void stm32l4x5_soc_realize(DeviceState *dev_soc, Error **errp)
{
    ERRP_GUARD();
    Stm32l4x5SocState *s = STM32L4X5_SOC(dev_soc);
    const Stm32l4x5SocClass *sc = STM32L4X5_SOC_GET_CLASS(dev_soc);
    MemoryRegion *system_memory = get_system_memory();
    DeviceState *armv7m, *dev;
    SysBusDevice *busdev;
    DriveInfo *dinfo;
    uint32_t pin_index;

    dinfo = drive_get(IF_MTD, 0, 0);
    if (dinfo) {
        qdev_prop_set_drive_err(DEVICE(&s->flash), "drive", blk_by_legacy_dinfo(dinfo),
                                &error_fatal);
    } else {
        error_setg(errp, "No MTD drive set");
        return;
    }

    if (!memory_region_init_ram(&s->sram1, OBJECT(dev_soc), "SRAM1", SRAM1_SIZE,
                                errp)) {
        return;
    }
    memory_region_add_subregion(system_memory, SRAM1_BASE_ADDRESS, &s->sram1);

    if (!memory_region_init_ram(&s->sram2, OBJECT(dev_soc), "SRAM2", SRAM2_SIZE,
                                errp)) {
        return;
    }
    memory_region_add_subregion(system_memory, SRAM2_BASE_ADDRESS, &s->sram2);

    object_initialize_child(OBJECT(dev_soc), "armv7m", &s->armv7m, TYPE_ARMV7M);
    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 96);
    qdev_prop_set_uint32(armv7m, "num-prio-bits", 4);
    qdev_prop_set_string(armv7m, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m4"));
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    qdev_connect_clock_in(armv7m, "cpuclk",
        qdev_get_clock_out(DEVICE(&(s->rcc)), "cortex-fclk-out"));
    qdev_connect_clock_in(armv7m, "refclk",
        qdev_get_clock_out(DEVICE(&(s->rcc)), "cortex-refclk-out"));
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }

    busdev = SYS_BUS_DEVICE(&s->flash);
    if (!sysbus_realize(busdev, errp)) {
        return;
    }
    sysbus_mmio_map(busdev, 0, FLASH_MMIO_BASE_ADDRESS);

    memory_region_add_subregion(system_memory, FLASH_BASE_ADDRESS, &s->flash.container);

    memory_region_init_alias(&s->flash_alias, OBJECT(dev_soc),
                             "flash_boot_alias", &s->flash.container, 0,
                             sc->flash_size);
    memory_region_add_subregion(system_memory, 0, &s->flash_alias);

    uint32_t memrmp = 0;
    if (s->flash.swapped_banks) {
        memrmp |= 0x100;
    }

    // if (!memory_region_init_rom(&s->flash, OBJECT(dev_soc), "flash",
    //                             sc->flash_size, errp)) {
    //     return;
    // }

    /* GPIOs */
    for (unsigned i = 0; i < NUM_GPIOS; i++) {
        g_autofree char *name = g_strdup_printf("%c", 'A' + i);
        dev = DEVICE(&s->gpio[i]);
        qdev_prop_set_string(dev, "name", name);
        qdev_prop_set_uint32(dev, "mode-reset",
                             stm32l4x5_gpio_cfg[i].moder_reset);
        qdev_prop_set_uint32(dev, "ospeed-reset",
                             stm32l4x5_gpio_cfg[i].ospeedr_reset);
        qdev_prop_set_uint32(dev, "pupd-reset",
                            stm32l4x5_gpio_cfg[i].pupdr_reset);
        busdev = SYS_BUS_DEVICE(&s->gpio[i]);
        g_free(name);
        name = g_strdup_printf("gpio%c-out", 'a' + i);
        qdev_connect_clock_in(DEVICE(&s->gpio[i]), "clk",
            qdev_get_clock_out(DEVICE(&(s->rcc)), name));
        if (!sysbus_realize(busdev, errp)) {
            return;
        }
        sysbus_mmio_map(busdev, 0, stm32l4x5_gpio_cfg[i].addr);
    }

    /* System configuration controller */
    dev = DEVICE(&s->syscfg);
    qdev_prop_set_uint32(dev, "memrmp", memrmp);
    busdev = SYS_BUS_DEVICE(&s->syscfg);
    if (!sysbus_realize(busdev, errp)) {
        return;
    }
    sysbus_mmio_map(busdev, 0, SYSCFG_ADDR);

    for (unsigned i = 0; i < NUM_GPIOS; i++) {
        for (unsigned j = 0; j < GPIO_NUM_PINS; j++) {
            pin_index = GPIO_NUM_PINS * i + j;
            qdev_connect_gpio_out(DEVICE(&s->gpio[i]), j,
                                  qdev_get_gpio_in(DEVICE(&s->syscfg),
                                  pin_index));
        }
    }

    /* I2C devices */
    for (unsigned i = 0; i < NUM_I2C; i++) {
        dev = DEVICE(&(s->i2c[i]));
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->i2c[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, i2c_addr[i]);
        // sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, i2c_irq[i * NUM_I2C]));
    }

    /* TSC */
    dev = DEVICE(&(s->tsc));
    qdev_prop_set_chr(dev, "chardev", serial_hd(2));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->tsc), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, TSC_ADDR);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, 77));

    /* GPIO_INT */
    dev = DEVICE(&(s->gpio_int));
    qdev_prop_set_chr(dev, "chardev", serial_hd(3));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpio_int), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(DEVICE(&s->exti), 6));

    /* EXTI device */
    busdev = SYS_BUS_DEVICE(&s->exti);
    if (!sysbus_realize(busdev, errp)) {
        return;
    }
    sysbus_mmio_map(busdev, 0, EXTI_ADDR);

    /* IRQs with fan-in that require an OR gate */
    for (unsigned i = 0; i < NUM_EXTI_OR_GATES; i++) {
        if (!object_property_set_int(OBJECT(&s->exti_or_gates[i]), "num-lines",
                                     exti_or_gates_num_lines_in[i], errp)) {
            return;
        }
        if (!qdev_realize(DEVICE(&s->exti_or_gates[i]), NULL, errp)) {
            return;
        }

        qdev_connect_gpio_out(DEVICE(&s->exti_or_gates[i]), 0,
            qdev_get_gpio_in(armv7m, exti_or_gates_out[i]));

        if (i < NUM_EXTI_SIMPLE_OR_GATES) {
            /* consecutive inputs for OR gates 23, 40, 63 */
            for (unsigned j = 0; j < exti_or_gates_num_lines_in[i]; j++) {
                sysbus_connect_irq(SYS_BUS_DEVICE(&s->exti),
                    exti_or_gates_first_line_in[i] + j,
                    qdev_get_gpio_in(DEVICE(&s->exti_or_gates[i]), j));
            }
        } else {
            /* non-consecutive inputs for OR gate 1 */
            for (unsigned j = 0; j < EXTI_OR_GATE1_NUM_LINES_IN; j++) {
                sysbus_connect_irq(SYS_BUS_DEVICE(&s->exti),
                    exti_or_gate1_lines_in[j],
                    qdev_get_gpio_in(DEVICE(&s->exti_or_gates[i]), j));
            }
        }
    }

    /* IRQs that don't require fan-in */
    for (unsigned i = 0; i < NUM_EXTI_IRQ; i++) {
        if (exti_irq[i] != -1) {
            sysbus_connect_irq(busdev, i,
                               qdev_get_gpio_in(armv7m, exti_irq[i]));
        }
    }

    for (unsigned i = 0; i < GPIO_NUM_PINS; i++) {
        qdev_connect_gpio_out(DEVICE(&s->syscfg), i,
                              qdev_get_gpio_in(DEVICE(&s->exti), i));
    }

    /* RCC device */
    busdev = SYS_BUS_DEVICE(&s->rcc);
    if (!sysbus_realize(busdev, errp)) {
        return;
    }
    sysbus_mmio_map(busdev, 0, RCC_BASE_ADDRESS);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, RCC_IRQ));

    /* PWR device */
    busdev = SYS_BUS_DEVICE(&s->pwr);
    if (!sysbus_realize(busdev, errp)) {
        return;
    }
    sysbus_mmio_map(busdev, 0, PWR_BASE_ADDRESS);

    /* RNG device */
    busdev = SYS_BUS_DEVICE(&s->rng);
    if (!sysbus_realize(busdev, errp)) {
        return;
    }
    sysbus_mmio_map(busdev, 0, RNG_BASE_ADDRESS);

    /* RTC device */
    busdev = SYS_BUS_DEVICE(&s->rtc);
    if (!sysbus_realize(busdev, errp)) {
        return;
    }
    sysbus_mmio_map(busdev, 0, RTC_BASE_ADDRESS);
    // sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, RCC_IRQ));

    create_unimplemented_device("OTP",       0x1FFF7000, 0x400);

    /* APB1 BUS */
    create_unimplemented_device("TIM2",      0x40000000, 0x400);
    create_unimplemented_device("TIM3",      0x40000400, 0x400);
    create_unimplemented_device("TIM4",      0x40000800, 0x400);
    create_unimplemented_device("TIM5",      0x40000C00, 0x400);
    create_unimplemented_device("TIM6",      0x40001000, 0x400);
    create_unimplemented_device("TIM7",      0x40001400, 0x400);
    /* RESERVED:    0x40001800, 0x1000 */
    create_unimplemented_device("RTC",       0x40002800, 0x400);
    create_unimplemented_device("WWDG",      0x40002C00, 0x400);
    create_unimplemented_device("IWDG",      0x40003000, 0x400);
    /* RESERVED:    0x40001800, 0x400 */
    create_unimplemented_device("SPI2",      0x40003800, 0x400);
    create_unimplemented_device("SPI3",      0x40003C00, 0x400);
    /* RESERVED:    0x40004000, 0x400 */
    create_unimplemented_device("USART2",    0x40004400, 0x400);
    create_unimplemented_device("USART3",    0x40004800, 0x400);
    create_unimplemented_device("UART4",     0x40004C00, 0x400);
    create_unimplemented_device("UART5",     0x40005000, 0x400);
    create_unimplemented_device("I2C1",      0x40005400, 0x400);
    create_unimplemented_device("I2C2",      0x40005800, 0x400);
    create_unimplemented_device("I2C3",      0x40005C00, 0x400);
    /* RESERVED:    0x40006000, 0x400 */
    create_unimplemented_device("CAN1",      0x40006400, 0x400);
    /* RESERVED:    0x40006800, 0x400 */
    create_unimplemented_device("DAC1",      0x40007400, 0x400);
    create_unimplemented_device("OPAMP",     0x40007800, 0x400);
    create_unimplemented_device("LPTIM1",    0x40007C00, 0x400);
    create_unimplemented_device("LPUART1",   0x40008000, 0x400);
    /* RESERVED:    0x40008400, 0x400 */
    create_unimplemented_device("SWPMI1",    0x40008800, 0x400);
    /* RESERVED:    0x40008C00, 0x800 */
    create_unimplemented_device("LPTIM2",    0x40009400, 0x400);
    /* RESERVED:    0x40009800, 0x6800 */

    /* APB2 BUS */
    create_unimplemented_device("VREFBUF",   0x40010030, 0x1D0);
    create_unimplemented_device("COMP",      0x40010200, 0x200);
    /* RESERVED:    0x40010800, 0x1400 */
    create_unimplemented_device("FIREWALL",  0x40011C00, 0x400);
    /* RESERVED:    0x40012000, 0x800 */
    create_unimplemented_device("SDMMC1",    0x40012800, 0x400);
    create_unimplemented_device("TIM1",      0x40012C00, 0x400);
    create_unimplemented_device("SPI1",      0x40013000, 0x400);
    create_unimplemented_device("TIM8",      0x40013400, 0x400);
    create_unimplemented_device("USART1",    0x40013800, 0x400);
    /* RESERVED:    0x40013C00, 0x400 */
    create_unimplemented_device("TIM15",     0x40014000, 0x400);
    create_unimplemented_device("TIM16",     0x40014400, 0x400);
    create_unimplemented_device("TIM17",     0x40014800, 0x400);
    /* RESERVED:    0x40014C00, 0x800 */
    create_unimplemented_device("SAI1",      0x40015400, 0x400);
    create_unimplemented_device("SAI2",      0x40015800, 0x400);
    /* RESERVED:    0x40015C00, 0x400 */
    create_unimplemented_device("DFSDM1",    0x40016000, 0x400);
    /* RESERVED:    0x40016400, 0x9C00 */

    /* AHB1 BUS */
    create_unimplemented_device("DMA1",      0x40020000, 0x400);
    create_unimplemented_device("DMA2",      0x40020400, 0x400);
    /* RESERVED:    0x40020800, 0x800 */
    /* RESERVED:    0x40021400, 0xC00 */
    /* RESERVED:    0x40022400, 0xC00 */
    create_unimplemented_device("CRC",       0x40023000, 0x400);
    /* RESERVED:    0x40023400, 0x400 */

    /* RESERVED:    0x40024400, 0x7FDBC00 */

    /* AHB2 BUS */
    /* RESERVED:    0x48002000, 0x7FDBC00 */
    create_unimplemented_device("OTG_FS",    0x50000000, 0x40000);
    create_unimplemented_device("ADC",       0x50040000, 0x400);
    /* RESERVED:    0x50040400, 0x20400 */

    /* AHB3 BUS */
    create_unimplemented_device("FMC",       0xA0000000, 0x1000);
    create_unimplemented_device("QUADSPI",   0xA0001000, 0x400);
}

static void stm32l4x5_soc_class_init(ObjectClass *klass, void *data)
{

    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32l4x5_soc_realize;
    /* Reason: Mapped at fixed location on the system bus */
    dc->user_creatable = false;
    /* No vmstate or reset required: device has no internal state */
}

static void stm32l4x5xc_soc_class_init(ObjectClass *oc, void *data)
{
    Stm32l4x5SocClass *ssc = STM32L4X5_SOC_CLASS(oc);

    ssc->flash_size = 256 * KiB;
}

static void stm32l4x5xe_soc_class_init(ObjectClass *oc, void *data)
{
    Stm32l4x5SocClass *ssc = STM32L4X5_SOC_CLASS(oc);

    ssc->flash_size = 512 * KiB;
}

static void stm32l4x5xg_soc_class_init(ObjectClass *oc, void *data)
{
    Stm32l4x5SocClass *ssc = STM32L4X5_SOC_CLASS(oc);

    ssc->flash_size = 1 * MiB;
}

static const TypeInfo stm32l4x5_soc_types[] = {
    {
        .name           = TYPE_STM32L4X5XC_SOC,
        .parent         = TYPE_STM32L4X5_SOC,
        .class_init     = stm32l4x5xc_soc_class_init,
    }, {
        .name           = TYPE_STM32L4X5XE_SOC,
        .parent         = TYPE_STM32L4X5_SOC,
        .class_init     = stm32l4x5xe_soc_class_init,
    }, {
        .name           = TYPE_STM32L4X5XG_SOC,
        .parent         = TYPE_STM32L4X5_SOC,
        .class_init     = stm32l4x5xg_soc_class_init,
    }, {
        .name           = TYPE_STM32L4X5_SOC,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(Stm32l4x5SocState),
        .instance_init  = stm32l4x5_soc_initfn,
        .class_size     = sizeof(Stm32l4x5SocClass),
        .class_init     = stm32l4x5_soc_class_init,
        .abstract       = true,
    }
};

DEFINE_TYPES(stm32l4x5_soc_types)
