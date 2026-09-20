/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <board.h>
#include <rtdevice.h>
#include "drv_common.h"
#include "drv_usart.h"

#ifdef BSP_USING_UART

#if !defined(BSP_USING_UART1) && !defined(BSP_USING_UART2)
#error "Enable at least one PY32 UART instance"
#endif

#define PY32_UART_ERROR_FLAGS (UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE)

struct py32_uart_config
{
    const char *name;
    USART_TypeDef *instance;
    IRQn_Type irq;
    GPIO_TypeDef *tx_port;
    rt_uint32_t tx_pin;
    rt_uint32_t tx_af;
    GPIO_TypeDef *rx_port;
    rt_uint32_t rx_pin;
    rt_uint32_t rx_af;
    rt_uint16_t rx_bufsz;
    rt_uint16_t tx_bufsz;
};

struct py32_uart
{
    struct rt_serial_device serial;
    UART_HandleTypeDef handle;
    const struct py32_uart_config *config;
    rt_uint16_t rx_mask;
};

enum
{
#ifdef BSP_USING_UART1
    UART1_INDEX,
#endif
#ifdef BSP_USING_UART2
    UART2_INDEX,
#endif
    UART_COUNT
};

/* Alternate functions and pin routing belong to the board, not the SoC driver. */
static const struct py32_uart_config uart_config[UART_COUNT] = {
#ifdef BSP_USING_UART1
    {
        "uart1",
        USART1,
        USART1_IRQn,
        PY32_UART1_TX_PORT,
        PY32_UART1_TX_PIN,
        PY32_UART1_TX_AF,
        PY32_UART1_RX_PORT,
        PY32_UART1_RX_PIN,
        PY32_UART1_RX_AF,
        BSP_UART1_RX_BUFSIZE,
        BSP_UART1_TX_BUFSIZE,
    },
#endif
#ifdef BSP_USING_UART2
    {
        "uart2",
        USART2,
        USART2_IRQn,
        PY32_UART2_TX_PORT,
        PY32_UART2_TX_PIN,
        PY32_UART2_TX_AF,
        PY32_UART2_RX_PORT,
        PY32_UART2_RX_PIN,
        PY32_UART2_RX_AF,
        BSP_UART2_RX_BUFSIZE,
        BSP_UART2_TX_BUFSIZE,
    },
#endif
};

static struct py32_uart uart_obj[UART_COUNT];

static void py32_uart_pin_init(const struct py32_uart_config *config)
{
    GPIO_InitTypeDef init = { 0 };

    py32_gpio_clock_enable(config->tx_port);
    py32_gpio_clock_enable(config->rx_port);

    if (config->instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
    }
    else if (config->instance == USART2)
    {
        __HAL_RCC_USART2_CLK_ENABLE();
    }

    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    init.Pin = config->tx_pin;
    init.Alternate = config->tx_af;
    HAL_GPIO_Init(config->tx_port, &init);

    init.Pin = config->rx_pin;
    init.Alternate = config->rx_af;
    HAL_GPIO_Init(config->rx_port, &init);
}

static rt_err_t py32_uart_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    struct py32_uart *uart;
    UART_InitTypeDef init = { 0 };

    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);
    uart = rt_container_of(serial, struct py32_uart, serial);

    init.BaudRate = cfg->baud_rate;
    init.Mode = UART_MODE_TX_RX;
    init.HwFlowCtl = UART_HWCONTROL_NONE;
    init.OverSampling = UART_OVERSAMPLING_16;

    switch (cfg->parity)
    {
    case PARITY_NONE:
        init.Parity = UART_PARITY_NONE;
        break;
    case PARITY_ODD:
        init.Parity = UART_PARITY_ODD;
        break;
    case PARITY_EVEN:
        init.Parity = UART_PARITY_EVEN;
        break;
    default:
        init.Parity = UART_PARITY_NONE;
        break;
    }

    /* The hardware word length includes parity. */
    if (cfg->data_bits == DATA_BITS_8)
    {
        init.WordLength = init.Parity == UART_PARITY_NONE ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;
    }
    else
    {
        init.WordLength = UART_WORDLENGTH_8B;
    }

    switch (cfg->stop_bits)
    {
    case STOP_BITS_1:
        init.StopBits = UART_STOPBITS_1;
        break;
    case STOP_BITS_2:
        init.StopBits = UART_STOPBITS_2;
        break;
    default:
        init.StopBits = UART_STOPBITS_1;
        break;
    }

    py32_uart_pin_init(uart->config);
    uart->handle.Init = init;
    if (HAL_UART_Init(&uart->handle) != HAL_OK)
    {
        return -RT_ERROR;
    }

    uart->rx_mask = (init.WordLength == UART_WORDLENGTH_8B && init.Parity != UART_PARITY_NONE) ? 0x7fu : 0xffu;
    return RT_EOK;
}

static rt_err_t py32_uart_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct py32_uart *uart;
    rt_ubase_t direction = (rt_ubase_t)arg;

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct py32_uart, serial);

    if (direction & (RT_DEVICE_FLAG_RX_BLOCKING | RT_DEVICE_FLAG_RX_NON_BLOCKING))
    {
        direction = RT_DEVICE_FLAG_INT_RX;
    }
    else if (direction & (RT_DEVICE_FLAG_TX_BLOCKING | RT_DEVICE_FLAG_TX_NON_BLOCKING))
    {
        direction = RT_DEVICE_FLAG_INT_TX;
    }

    switch (cmd)
    {
    case RT_DEVICE_CTRL_SET_INT:
        NVIC_SetPriority(uart->config->irq, 5);
        NVIC_EnableIRQ(uart->config->irq);
        if (direction == RT_DEVICE_FLAG_INT_RX)
        {
            __HAL_UART_ENABLE_IT(&uart->handle, UART_IT_RXNE);
        }
        else if (direction == RT_DEVICE_FLAG_INT_TX)
        {
            __HAL_UART_ENABLE_IT(&uart->handle, UART_IT_TXE);
        }
        break;

    case RT_DEVICE_CTRL_CLR_INT:
        NVIC_DisableIRQ(uart->config->irq);
        if (direction == RT_DEVICE_FLAG_INT_RX)
        {
            __HAL_UART_DISABLE_IT(&uart->handle, UART_IT_RXNE);
        }
        else if (direction == RT_DEVICE_FLAG_INT_TX)
        {
            __HAL_UART_DISABLE_IT(&uart->handle, UART_IT_TXE);
            __HAL_UART_DISABLE_IT(&uart->handle, UART_IT_TC);
        }
        break;

    case RT_DEVICE_CTRL_CONFIG:
        return py32_uart_control(serial, RT_DEVICE_CTRL_SET_INT, (void *)direction);

    case RT_DEVICE_CHECK_OPTMODE:
        return RT_SERIAL_TX_BLOCKING_BUFFER;

    case RT_DEVICE_CTRL_CLOSE:
        return HAL_UART_DeInit(&uart->handle) == HAL_OK ? RT_EOK : -RT_ERROR;

    default:
        break;
    }
    return RT_EOK;
}

static int py32_uart_putc(struct rt_serial_device *serial, char c)
{
    struct py32_uart *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct py32_uart, serial);

    __HAL_UART_CLEAR_FLAG(&uart->handle, UART_FLAG_TC);
    uart->handle.Instance->DR = (rt_uint8_t)c;
    while (__HAL_UART_GET_FLAG(&uart->handle, UART_FLAG_TC) == RESET)
    {
    }

    return 1;
}

static int py32_uart_getc(struct rt_serial_device *serial)
{
    struct py32_uart *uart;
    rt_uint32_t status;
    rt_uint32_t data;

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct py32_uart, serial);

    status = uart->handle.Instance->SR;
    if (status & (UART_FLAG_RXNE | PY32_UART_ERROR_FLAGS))
    {
        /* Reading SR followed by DR clears RXNE and the receive error flags. */
        data = uart->handle.Instance->DR;
        if (status & UART_FLAG_RXNE)
        {
            return data & uart->rx_mask;
        }
    }

    return -1;
}

static rt_ssize_t py32_uart_transmit(struct rt_serial_device *serial, rt_uint8_t *buf,
                                    rt_size_t size, rt_uint32_t tx_flag)
{
    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(buf != RT_NULL);

    py32_uart_control(serial, RT_DEVICE_CTRL_SET_INT, (void *)(rt_ubase_t)tx_flag);
    return size;
}

static void py32_uart_isr(struct py32_uart *uart)
{
    if ((__HAL_UART_GET_IT_SOURCE(&uart->handle, UART_IT_RXNE) != RESET) &&
        (uart->handle.Instance->SR & (UART_FLAG_RXNE | PY32_UART_ERROR_FLAGS)))
    {
        int ch = py32_uart_getc(&uart->serial);
        if (ch >= 0)
        {
            rt_uint8_t data = ch;
            rt_hw_serial_control_isr(&uart->serial, RT_HW_SERIAL_CTRL_PUTC, &data);
            rt_hw_serial_isr(&uart->serial, RT_SERIAL_EVENT_RX_IND);
        }
    }

    if ((__HAL_UART_GET_FLAG(&uart->handle, UART_FLAG_TXE) != RESET) &&
        (__HAL_UART_GET_IT_SOURCE(&uart->handle, UART_IT_TXE) != RESET))
    {
        rt_uint8_t data;
        if (rt_hw_serial_control_isr(&uart->serial, RT_HW_SERIAL_CTRL_GETC, &data) == RT_EOK)
        {
            uart->handle.Instance->DR = data;
        }
        else
        {
            __HAL_UART_DISABLE_IT(&uart->handle, UART_IT_TXE);
            __HAL_UART_ENABLE_IT(&uart->handle, UART_IT_TC);
        }
    }

    if ((__HAL_UART_GET_FLAG(&uart->handle, UART_FLAG_TC) != RESET) &&
        (__HAL_UART_GET_IT_SOURCE(&uart->handle, UART_IT_TC) != RESET))
    {
        __HAL_UART_DISABLE_IT(&uart->handle, UART_IT_TC);
        rt_hw_serial_isr(&uart->serial, RT_SERIAL_EVENT_TX_DONE);
        __HAL_UART_CLEAR_FLAG(&uart->handle, UART_FLAG_TC);
    }
}

#ifdef BSP_USING_UART1
void USART1_IRQHandler(void)
{
    rt_interrupt_enter();
    py32_uart_isr(&uart_obj[UART1_INDEX]);
    rt_interrupt_leave();
}
#endif

#ifdef BSP_USING_UART2
void USART2_IRQHandler(void)
{
    rt_interrupt_enter();
    py32_uart_isr(&uart_obj[UART2_INDEX]);
    rt_interrupt_leave();
}
#endif

static const struct rt_uart_ops py32_uart_ops = {
    .configure = py32_uart_configure,
    .control = py32_uart_control,
    .putc = py32_uart_putc,
    .getc = py32_uart_getc,
    .transmit = py32_uart_transmit,
};

int rt_hw_usart_init(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_err_t result;
    rt_size_t i;

    for (i = 0; i < UART_COUNT; i++)
    {
        uart_obj[i].config = &uart_config[i];
        uart_obj[i].handle.Instance = uart_config[i].instance;
        uart_obj[i].serial.ops = &py32_uart_ops;
        uart_obj[i].serial.config = config;
        uart_obj[i].serial.config.rx_bufsz = uart_config[i].rx_bufsz;
        uart_obj[i].serial.config.tx_bufsz = uart_config[i].tx_bufsz;

        result = rt_hw_serial_register(&uart_obj[i].serial, uart_config[i].name,
                                       RT_DEVICE_FLAG_RDWR, RT_NULL);
        if (result != RT_EOK)
        {
            return result;
        }
    }

    return RT_EOK;
}

#endif /* BSP_USING_UART */
