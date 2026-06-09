/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for non-os stm32f10x.
 * Created on: 2015-04-28
 */

#include "elog.h"
#include "Project_BuildGuard.h"
#include <stdio.h>
#include <stm32f10x.h>

#define ELOG_UART_TX_WAIT_LOOP ((uint32_t)100000UL)

void IWDG_Feed(void);
uint32_t SysTime_Get10msTickCount(void);

static USART_TypeDef *elog_port_uart(void)
{
#if PROJECT_CFG_RUNTIME_LOG_UART == 2
    return USART2;
#elif PROJECT_CFG_RUNTIME_LOG_UART == 3
    return USART3;
#else
    return USART1;
#endif
}

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void) {
    ElogErrCode result = ELOG_NO_ERR;

    return result;
}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size) {
#if PROJECT_CFG_RUNTIME_LOG_ENABLE
    USART_TypeDef *uart = elog_port_uart();
    size_t i;

    if (log == NULL) {
        return;
    }

    for (i = 0U; i < size; i++) {
        uint32_t wait_loop = ELOG_UART_TX_WAIT_LOOP;

        while ((USART_GetFlagStatus(uart, USART_FLAG_TXE) == RESET) && (wait_loop > 0U)) {
            IWDG_Feed();
            wait_loop--;
        }
        if (wait_loop == 0U) {
            break;
        }
        USART_SendData(uart, (uint8_t)log[i]);
    }
#else
    (void)log;
    (void)size;
#endif
}

/**
 * output lock
 */
void elog_port_output_lock(void) {
}

/**
 * output unlock
 */
void elog_port_output_unlock(void) {
}

/**
 * get current time interface
 *
 * @return current time
 */
const char *elog_port_get_time(void) {
    static char time_buf[16];
    uint32_t tick_10ms = SysTime_Get10msTickCount();

    snprintf(time_buf, sizeof(time_buf), "%lu.%02lu",
             (unsigned long)(tick_10ms / 100UL),
             (unsigned long)(tick_10ms % 100UL));
    return time_buf;
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void) {
    return "";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void) {
    return "";
}
