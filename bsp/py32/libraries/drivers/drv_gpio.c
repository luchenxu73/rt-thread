/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtdevice.h>
#include "drv_common.h"
#include "drv_gpio.h"

#ifdef BSP_USING_GPIO

static GPIO_TypeDef *const gpio_ports[] =
{
    GPIOA, GPIOB, GPIOC, GPIOD, GPIOE
};

static GPIO_TypeDef *py32_pin_port(rt_base_t pin)
{
    if ((pin < 0) || (pin >= (rt_base_t)(sizeof(gpio_ports) / sizeof(gpio_ports[0]) * 16)))
    {
        return RT_NULL;
    }

    return gpio_ports[pin / 16];
}

static void py32_pin_mode(struct rt_device *device, rt_base_t pin, rt_uint8_t mode)
{
    GPIO_TypeDef *port = py32_pin_port(pin);
    GPIO_InitTypeDef init = {0};

    if (port == RT_NULL)
    {
        return;
    }

    init.Pin = 1u << (pin % 16);
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_HIGH;

    switch (mode)
    {
    case PIN_MODE_OUTPUT:
        init.Mode = GPIO_MODE_OUTPUT_PP;
        break;
    case PIN_MODE_OUTPUT_OD:
        init.Mode = GPIO_MODE_OUTPUT_OD;
        break;
    case PIN_MODE_INPUT:
        init.Mode = GPIO_MODE_INPUT;
        break;
    case PIN_MODE_INPUT_PULLUP:
        init.Mode = GPIO_MODE_INPUT;
        init.Pull = GPIO_PULLUP;
        break;
    case PIN_MODE_INPUT_PULLDOWN:
        init.Mode = GPIO_MODE_INPUT;
        init.Pull = GPIO_PULLDOWN;
        break;
    default:
        return;
    }

    HAL_GPIO_Init(port, &init);
}

static void py32_pin_write(struct rt_device *device, rt_base_t pin, rt_uint8_t value)
{
    GPIO_TypeDef *port = py32_pin_port(pin);

    if (port != RT_NULL)
    {
        HAL_GPIO_WritePin(port, 1u << (pin % 16), value == PIN_LOW ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}

static rt_ssize_t py32_pin_read(struct rt_device *device, rt_base_t pin)
{
    GPIO_TypeDef *port = py32_pin_port(pin);

    if (port == RT_NULL)
    {
        return -RT_EINVAL;
    }

    return HAL_GPIO_ReadPin(port, 1u << (pin % 16)) == GPIO_PIN_RESET ? PIN_LOW : PIN_HIGH;
}

static rt_base_t py32_pin_get(const char *name)
{
    rt_size_t length;
    rt_base_t port;
    rt_base_t number;

    if (name == RT_NULL)
    {
        return -RT_EINVAL;
    }

    length = rt_strlen(name);
    if ((length < 4) || (length > 5) || (name[0] != 'P') || (name[2] != '.') ||
        (name[1] < 'A') || (name[1] > 'E') || (name[3] < '0') || (name[3] > '9'))
    {
        return -RT_EINVAL;
    }

    port = name[1] - 'A';
    number = name[3] - '0';
    if (length == 5)
    {
        if ((name[4] < '0') || (name[4] > '9'))
        {
            return -RT_EINVAL;
        }
        number = number * 10 + name[4] - '0';
    }

    if (number > 15)
    {
        return -RT_EINVAL;
    }

    return port * 16 + number;
}

static rt_err_t py32_pin_attach_irq(struct rt_device *device, rt_base_t pin,
                                  rt_uint8_t mode, void (*hdr)(void *args), void *args)
{
    return -RT_ENOSYS;
}

static rt_err_t py32_pin_detach_irq(struct rt_device *device, rt_base_t pin)
{
    return -RT_ENOSYS;
}

static rt_err_t py32_pin_irq_enable(struct rt_device *device, rt_base_t pin, rt_uint8_t enabled)
{
    return -RT_ENOSYS;
}

static const struct rt_pin_ops py32_pin_ops =
{
    .pin_mode = py32_pin_mode,
    .pin_write = py32_pin_write,
    .pin_read = py32_pin_read,
    .pin_attach_irq = py32_pin_attach_irq,
    .pin_detach_irq = py32_pin_detach_irq,
    .pin_irq_enable = py32_pin_irq_enable,
    .pin_get = py32_pin_get,
};

int rt_hw_pin_init(void)
{
    rt_size_t i;

    for (i = 0; i < sizeof(gpio_ports) / sizeof(gpio_ports[0]); i++)
    {
        py32_gpio_clock_enable(gpio_ports[i]);
    }

    return rt_device_pin_register("pin", &py32_pin_ops, RT_NULL);
}

#endif /* BSP_USING_GPIO */
