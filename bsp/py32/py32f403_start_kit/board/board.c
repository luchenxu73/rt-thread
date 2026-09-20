/*
 * Copyright (c) 2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <board.h>
#include <rthw.h>
#include <rtdevice.h>
#include <drv_usart.h>

extern __IO uint32_t uwTick;
static rt_uint32_t systick_ms = 1;

HAL_StatusTypeDef HAL_InitTick(uint32_t priority)
{
    RT_UNUSED(priority);
    SystemCoreClockUpdate();
    HAL_SYSTICK_Config(SystemCoreClock / RT_TICK_PER_SECOND);
    NVIC_SetPriority(SysTick_IRQn, 0xFF);

    systick_ms = 1000U / RT_TICK_PER_SECOND;
    if (systick_ms == 0U)
    {
        systick_ms = 1U;
    }
    return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
    {
        HAL_IncTick();
    }
    return uwTick;
}

void HAL_IncTick(void)
{
    uwTick += systick_ms;
}

void rt_hw_us_delay(rt_uint32_t us)
{
    rt_uint64_t ticks;
    rt_uint32_t told, tnow, tcnt = 0;
    rt_uint32_t reload = SysTick->LOAD + 1;

    ticks = us * (reload / (1000000 / RT_TICK_PER_SECOND));
    told = SysTick->VAL;
    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;
            }
            else
            {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            if (tcnt >= ticks)
            {
                break;
            }
        }
    }
}

void HAL_Delay(uint32_t delay)
{
    if (rt_thread_self())
    {
        rt_thread_mdelay(delay);
    }
    else
    {
        for (rt_uint32_t count = 0; count < delay; count++)
        {
            rt_hw_us_delay(1000U);
        }
    }
}

void HAL_SuspendTick(void)
{
    /* SysTick belongs to RT-Thread. HAL callers must not stop the OS tick. */
}

void HAL_ResumeTick(void)
{
    /* Tick configuration and low-power accounting belong to the kernel. */
}

void SysTick_Handler(void)
{
    rt_interrupt_enter();
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
    {
        HAL_IncTick();
    }
    rt_tick_increase();
    rt_interrupt_leave();
}

static HAL_StatusTypeDef py32_clock_config(void)
{
    RCC_OscInitTypeDef oscillator = { 0 };
    RCC_ClkInitTypeDef clocks = { 0 };

    /* Same conservative HSE setup as the vendor RCC_HSE_Output example. */
    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.HSEFreq = RCC_HSE_16_32MHz;
    oscillator.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK)
    {
        return HAL_ERROR;
    }

    clocks.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV1;
    clocks.APB2CLKDivider = RCC_HCLK_DIV2;
    return HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_0);
}

void rt_hw_board_init(void)
{
    HAL_Init();
    if (py32_clock_config() != HAL_OK)
    {
        /* Stop here for debugger inspection if the board's HSE fails. */
        RT_ASSERT(0);
        while (1)
        {
        }
    }
#ifdef RT_USING_HEAP
    rt_system_heap_init(HEAP_BEGIN, HEAP_END);
#endif
#ifdef BSP_USING_GPIO
    rt_hw_pin_init();
#endif
#ifdef BSP_USING_UART
    rt_hw_usart_init();
#endif
#ifdef RT_USING_CONSOLE
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
}
