/*
 * Copyright (c) 2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

int main(void)
{
    rt_kprintf("PY32F403 Start Kit (PY32F403V1DT6)\n");
#ifdef BSP_USING_GPIO
    /* The Start Kit green LED is active low. */
    rt_pin_write(LED_GREEN_PIN, LED_GREEN_OFF);
    rt_pin_mode(LED_GREEN_PIN, PIN_MODE_OUTPUT);
    while (1)
    {
        rt_pin_write(LED_GREEN_PIN, LED_GREEN_ON);
        rt_thread_mdelay(500);
        rt_pin_write(LED_GREEN_PIN, LED_GREEN_OFF);
        rt_thread_mdelay(500);
    }
#endif
    return 0;
}
