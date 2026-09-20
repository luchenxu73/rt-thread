/*
 * Copyright (c) 2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <board.h>
#include <rthw.h>
#include <rtdevice.h>
#include <drv_usart.h>

static rt_uint32_t boot_last_cycle;
static rt_uint32_t boot_cycle_remainder;
static rt_uint32_t boot_cycles_per_ms;
static rt_uint32_t boot_milliseconds;
static rt_tick_t hal_tick_origin;
static rt_bool_t hal_scheduler_time;

/* Board initialization runs with interrupts disabled. DWT keeps HAL oscillator
 * timeouts bounded before the scheduler can supply a tick interrupt. */
HAL_StatusTypeDef HAL_InitTick(uint32_t priority)
{
    RT_UNUSED(priority);
    if (boot_cycles_per_ms != 0U)
    {
        HAL_GetTick();
    }
    boot_last_cycle = DWT->CYCCNT;
    boot_cycle_remainder = 0U;
    boot_cycles_per_ms = SystemCoreClock / 1000U;
    /* RT-Thread owns SysTick; the HAL must not reconfigure it. */
    return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    rt_uint32_t now;

    if (!hal_scheduler_time)
    {
        rt_uint32_t cycle = DWT->CYCCNT;
        rt_uint64_t elapsed = (rt_uint32_t)(cycle - boot_last_cycle);

        boot_last_cycle = cycle;
        if (boot_cycles_per_ms != 0U)
        {
            elapsed += boot_cycle_remainder;
            boot_milliseconds += elapsed / boot_cycles_per_ms;
            boot_cycle_remainder = elapsed % boot_cycles_per_ms;
        }
        if (rt_thread_self() != RT_NULL)
        {
            hal_tick_origin = rt_tick_get();
            hal_scheduler_time = RT_TRUE;
        }
        now = boot_milliseconds;
    }
    else
    {
        now = boot_milliseconds + (rt_uint64_t)(rt_tick_get() - hal_tick_origin)
                                  * 1000U / RT_TICK_PER_SECOND;
    }
    rt_hw_interrupt_enable(level);
    return now;
}

void rt_hw_us_delay(rt_uint32_t us)
{
    rt_uint32_t ticks_per_us = SystemCoreClock / 1000000U;

    /* Short chunks keep unsigned cycle-counter arithmetic valid across wrap. */
    while (us != 0U)
    {
        rt_uint32_t chunk = us > 1000U ? 1000U : us;
        rt_uint32_t start = DWT->CYCCNT;
        rt_uint32_t ticks = chunk * ticks_per_us;

        while ((rt_uint32_t)(DWT->CYCCNT - start) < ticks)
        {
        }
        us -= chunk;
    }
}

void HAL_Delay(uint32_t delay)
{
    /* Also usable before the scheduler starts and while interrupts are masked. */
    while (delay-- != 0U)
    {
        rt_hw_us_delay(1000U);
    }
}

void SysTick_Handler(void)
{
    rt_interrupt_enter();
    if (!hal_scheduler_time)
    {
        /* Switch on the first scheduler tick, before the boot DWT can wrap. */
        HAL_GetTick();
    }
    rt_tick_increase();
    rt_interrupt_leave();
}

static HAL_StatusTypeDef py32_clock_config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clocks = {0};

    /* Same conservative HSE setup as the vendor RCC_HSE_Output example. */
    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.HSEFreq = RCC_HSE_16_32MHz;
    oscillator.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK)
    {
        return HAL_ERROR;
    }

    clocks.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV1;
    clocks.APB2CLKDivider = RCC_HCLK_DIV2;
    return HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_0);
}

void rt_hw_board_init(void)
{
    SystemCoreClockUpdate();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    HAL_Init();
    if (py32_clock_config() != HAL_OK)
    {
        /* Stop here for debugger inspection if the board's HSE fails. */
        RT_ASSERT(0);
        while (1)
        {
        }
    }
    SystemCoreClockUpdate();

    if (SysTick_Config(SystemCoreClock / RT_TICK_PER_SECOND) != 0U)
    {
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
