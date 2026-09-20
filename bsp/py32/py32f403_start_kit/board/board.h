/*
 * Copyright (c) 2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <rtthread.h>
#include <py32f4xx_hal.h>
#include <drv_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PY32_SRAM_BEGIN          0x20000000U
#define PY32_SRAM_SIZE           (64U * 1024U)
#define PY32_SRAM_END            (PY32_SRAM_BEGIN + PY32_SRAM_SIZE)

#define LED_GREEN_PIN            GET_PIN(A, 1)
#define LED_GREEN_ON             PIN_LOW
#define LED_GREEN_OFF            PIN_HIGH
#define USER_BUTTON_PIN          GET_PIN(A, 0)

#ifdef __ARMCC_VERSION
extern unsigned char Image$$RW_IRAM1$$ZI$$Limit;
#define HEAP_BEGIN              ((void *)&Image$$RW_IRAM1$$ZI$$Limit)
#else
extern unsigned char __bss_end__;
#define HEAP_BEGIN              ((void *)&__bss_end__)
#endif
#define HEAP_END                ((void *)PY32_SRAM_END)

#define PY32_UART1_TX_PORT       GPIOA
#define PY32_UART1_TX_PIN        GPIO_PIN_9
#define PY32_UART1_TX_AF         GPIO_AF2_USART1
#define PY32_UART1_RX_PORT       GPIOA
#define PY32_UART1_RX_PIN        GPIO_PIN_10
#define PY32_UART1_RX_AF         GPIO_AF2_USART1
#define PY32_UART2_TX_PORT       GPIOA
#define PY32_UART2_TX_PIN        GPIO_PIN_2
#define PY32_UART2_TX_AF         GPIO_AF2_USART2
#define PY32_UART2_RX_PORT       GPIOA
#define PY32_UART2_RX_PIN        GPIO_PIN_3
#define PY32_UART2_RX_AF         GPIO_AF2_USART2

void rt_hw_board_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_H__ */
