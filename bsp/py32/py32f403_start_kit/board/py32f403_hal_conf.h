/*
 * Copyright (c) 2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PY32F403_HAL_CONF_H__
#define __PY32F403_HAL_CONF_H__

#include <rtconfig.h>

#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#ifdef BSP_USING_UART
#define HAL_UART_MODULE_ENABLED
#endif

#ifndef HSE_VALUE
#define HSE_VALUE                24000000U
#endif
#define HSE_STARTUP_TIMEOUT      100U
#define HSI_VALUE                8000000U
#define HSI48_VALUE              48000000U
#define LSI_VALUE                40000U
#define LSE_VALUE                32768U
#define LSE_STARTUP_TIMEOUT      5000U
#define VDD_VALUE                3300U
#define TICK_INT_PRIORITY        7U
#define USE_RTOS                 0U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U

#include <py32f403_hal_rcc.h>
#include <py32f403_hal_gpio.h>
#include <py32f403_hal_dma.h>
#include <py32f403_hal_cortex.h>
#include <py32f403_hal_flash.h>
#include <py32f403_hal_pwr.h>
#ifdef HAL_UART_MODULE_ENABLED
#include <py32f403_hal_uart.h>
#endif

#define assert_param(expr)       ((void)0U)

#endif /* __PY32F403_HAL_CONF_H__ */
