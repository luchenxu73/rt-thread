/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DRV_GPIO_H__
#define DRV_GPIO_H__

#include <rtthread.h>
#include <py32f4xx.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GPIO numbers identify the peripheral pin, not its physical package pin. */
#define GET_PIN(PORT, PIN) \
    ((rt_base_t)((GPIO##PORT##_BASE - GPIOA_BASE) / 0x400u * 16u + (PIN)))

int rt_hw_pin_init(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_GPIO_H__ */
