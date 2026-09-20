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

#ifndef RT_USING_SERIAL_V1
#error "The PY32 UART driver requires RT_USING_SERIAL_V1"
#endif

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
static const struct py32_uart_config uart_config[UART_COUNT] =
{
#ifdef BSP_USING_UART1
    {
        "uart1", USART1, USART1_IRQn,
        PY32_UART1_TX_PORT, PY32_UART1_TX_PIN, PY32_UART1_TX_AF,
        PY32_UART1_RX_PORT, PY32_UART1_RX_PIN, PY32_UART1_RX_AF,
    },
#endif
#ifdef BSP_USING_UART2
    {
        "uart2", USART2, USART2_IRQn,
        PY32_UART2_TX_PORT, PY32_UART2_TX_PIN, PY32_UART2_TX_AF,
        PY32_UART2_RX_PORT, PY32_UART2_RX_PIN, PY32_UART2_RX_AF,
    },
#endif
};

static struct py32_uart uart_obj[UART_COUNT];

static void py32_uart_pin_init(const struct py32_uart_config *config)
{
    GPIO_InitTypeDef init = {0};

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
    UART_InitTypeDef init = {0};
    rt_uint32_t pclk;
    rt_uint32_t divider;

    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);
    uart = rt_container_of(serial, struct py32_uart, serial);

    if ((cfg->baud_rate == 0) || !IS_UART_BAUDRATE(cfg->baud_rate) ||
        (cfg->flowcontrol != RT_SERIAL_FLOWCONTROL_NONE) ||
        (cfg->bit_order != BIT_ORDER_LSB) || (cfg->invert != NRZ_NORMAL))
    {
        return -RT_EINVAL;
    }

    pclk = uart->config->instance == USART1 ? HAL_RCC_GetPCLK2Freq() : HAL_RCC_GetPCLK1Freq();
    divider = UART_BRR_SAMPLING16(pclk, cfg->baud_rate);
    if ((divider < 16u) || (divider > 0xffffu))
    {
        return -RT_EINVAL;
    }

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
        return -RT_EINVAL;
    }

    /* The hardware word length includes parity; Serial V1 transfers bytes. */
    if (cfg->data_bits == DATA_BITS_8)
    {
        init.WordLength = cfg->parity == PARITY_NONE ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;
    }
    else if ((cfg->data_bits == DATA_BITS_7) && (cfg->parity != PARITY_NONE))
    {
        init.WordLength = UART_WORDLENGTH_8B;
    }
    else
    {
        return -RT_EINVAL;
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
        return -RT_EINVAL;
    }

    py32_uart_pin_init(uart->config);
    uart->handle.Init = init;
    if (HAL_UART_Init(&uart->handle) != HAL_OK)
    {
        return -RT_ERROR;
    }

    uart->rx_mask = cfg->data_bits == DATA_BITS_7 ? 0x7fu : 0xffu;
    return RT_EOK;
}

static rt_err_t py32_uart_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct py32_uart *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct py32_uart, serial);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_SET_INT:
        if ((rt_ubase_t)arg != RT_DEVICE_FLAG_INT_RX)
        {
            return -RT_ENOSYS;
        }
        NVIC_SetPriority(uart->config->irq, 5);
        NVIC_ClearPendingIRQ(uart->config->irq);
        __HAL_UART_ENABLE_IT(&uart->handle, UART_IT_RXNE);
        NVIC_EnableIRQ(uart->config->irq);
        return RT_EOK;

    case RT_DEVICE_CTRL_CLR_INT:
        if ((rt_ubase_t)arg != RT_DEVICE_FLAG_INT_RX)
        {
            return -RT_ENOSYS;
        }
        __HAL_UART_DISABLE_IT(&uart->handle, UART_IT_RXNE);
        NVIC_DisableIRQ(uart->config->irq);
        NVIC_ClearPendingIRQ(uart->config->irq);
        return RT_EOK;

    case RT_DEVICE_CTRL_CLOSE:
        __HAL_UART_DISABLE_IT(&uart->handle, UART_IT_RXNE);
        NVIC_DisableIRQ(uart->config->irq);
        NVIC_ClearPendingIRQ(uart->config->irq);
        return HAL_UART_DeInit(&uart->handle) == HAL_OK ? RT_EOK : -RT_ERROR;

    default:
        return -RT_ENOSYS;
    }
}

static int py32_uart_putc(struct rt_serial_device *serial, char c)
{
    struct py32_uart *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = rt_container_of(serial, struct py32_uart, serial);

    while (__HAL_UART_GET_FLAG(&uart->handle, UART_FLAG_TXE) == RESET)
    {
    }
    uart->handle.Instance->DR = (rt_uint8_t)c;

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

static void py32_uart_isr(struct py32_uart *uart)
{
    if ((__HAL_UART_GET_IT_SOURCE(&uart->handle, UART_IT_RXNE) != RESET) &&
        (uart->handle.Instance->SR & (UART_FLAG_RXNE | PY32_UART_ERROR_FLAGS)))
    {
        /* Serial V1 drains getc(), which also acknowledges receive errors. */
        rt_hw_serial_isr(&uart->serial, RT_SERIAL_EVENT_RX_IND);
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

static const struct rt_uart_ops py32_uart_ops =
{
    .configure = py32_uart_configure,
    .control = py32_uart_control,
    .putc = py32_uart_putc,
    .getc = py32_uart_getc,
    .dma_transmit = RT_NULL,
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

        result = rt_hw_serial_register(&uart_obj[i].serial, uart_config[i].name,
                                      RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX, RT_NULL);
        if (result != RT_EOK)
        {
            return result;
        }
    }

    return RT_EOK;
}

#endif /* BSP_USING_UART */
