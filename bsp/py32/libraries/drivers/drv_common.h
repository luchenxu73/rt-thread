/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DRV_COMMON_H__
#define DRV_COMMON_H__

#include <rtthread.h>
#include <board.h>

/* Like the STM32 HAL drivers, use the capabilities exposed by the board's HAL. */
rt_inline void py32_gpio_clock_enable(GPIO_TypeDef *port)
{
#if defined(GPIOA) && defined(__HAL_RCC_GPIOA_CLK_ENABLE)
    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOB) && defined(__HAL_RCC_GPIOB_CLK_ENABLE)
    if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOC) && defined(__HAL_RCC_GPIOC_CLK_ENABLE)
    if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOD) && defined(__HAL_RCC_GPIOD_CLK_ENABLE)
    if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOE) && defined(__HAL_RCC_GPIOE_CLK_ENABLE)
    if (port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOF) && defined(__HAL_RCC_GPIOF_CLK_ENABLE)
    if (port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
        return;
    }
#endif
}

#endif /* DRV_COMMON_H__ */
