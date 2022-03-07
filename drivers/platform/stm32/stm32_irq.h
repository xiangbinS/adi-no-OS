/***************************************************************************//**
 *   @file   stm32/stm32_irq.h
 *   @brief  Header file for stm32 irq specifics.
 *   @author Darius Berghe (darius.berghe@analog.com)
********************************************************************************
 * Copyright 2022(c) Analog Devices, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *  - The use of this software may or may not infringe the patent rights
 *    of one or more patent holders.  This license does not release you
 *    from the requirement that you obtain separate licenses from these
 *    patent holders to use this software.
 *  - Use of the software either in source or binary form, must be run
 *    on or directly connected to an Analog Devices Inc. component.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#ifndef STM32_IRQ_H
#define STM32_IRQ_H

#include "no-os/irq.h"
#include "stm32_hal.h"

/**
 * @enum stm32_irq_source
 * @brief Possible source peripheral of interupt.
 */
enum stm32_irq_source {
	STM32_INVALID_IRQ,
	STM32_EXTI_IRQ,
	STM32_UART_IRQ,
};

/**
 * @struct stm32_callback
 * @brief stm32 specific callback configuration structure.
 */

struct stm32_callback {
	/** Callback function pointer. Used as the 3rd parameter of HAL_*_RegisterCallback(). */
	union {
		void (*exti)(void);
		void (*uart)(UART_HandleTypeDef *);
	} callback;
	/** Specific event that triggers the callback. Used as the 2nd parameter of HAL_*_RegisterCallback(). */
	uint32_t event;
	/** Interrupt source peripheral. */
	enum stm32_irq_source source;
};

/**
 * @brief stm32 platform specific irq platform ops structure
 */
extern const struct irq_platform_ops stm32_irq_ops;

#endif
