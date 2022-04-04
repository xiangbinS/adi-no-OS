/***************************************************************************//**
 *   @file   iio_adxl355.h
 *   @brief  Header file of IIO ADXL355 Driver.
 *   @author RBolboac (ramona.bolboaca@analog.com)
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
#ifndef IIO_ADXL355_H
#define IIO_ADXL355_H

/******************************************************************************/
/***************************** Include Files **********************************/
/******************************************************************************/
#include "iio.h"
#include "no_os_irq.h"

/******************************************************************************/
/********************** Macros and Constants Definitions **********************/
/******************************************************************************/
#define TRIG_MAX_NAME_SIZE 20

extern struct iio_trigger adxl355_iio_trigger_desc;

/******************************************************************************/
/*************************** Types Declarations *******************************/
/******************************************************************************/
struct adxl355_iio_dev {
	struct adxl355_dev *adxl355_dev;
	struct iio_device *iio_dev;
	int adxl355_hpf_3db_table[7][2];
	uint32_t active_channels;
	uint8_t no_of_active_channels;
};

struct adxl355_iio_dev_init_param {
	struct adxl355_init_param *adxl355_dev_init;
};

struct adxl355_iio_trig {
	struct iio_desc             **iio_desc;
	struct no_os_irq_ctrl_desc	*irq_ctrl;
	struct no_os_irq_init_param *irq_init_param;
	char		                name[TRIG_MAX_NAME_SIZE + 1];
};

struct adxl355_iio_trig_init_param {
	struct iio_desc		        **iio_desc;
	struct no_os_irq_ctrl_desc	*irq_ctrl;
	struct no_os_irq_init_param *irq_init_param;
	const char                  *name;
};

/******************************************************************************/
/************************ Functions Declarations ******************************/
/******************************************************************************/
int adxl355_iio_init(struct adxl355_iio_dev **iio_dev,
		     struct adxl355_iio_dev_init_param *init_param);

int adxl355_iio_remove(struct adxl355_iio_dev *desc);

int iio_adxl355_trigger_init(struct adxl355_iio_trig **iio_trig,
			     struct adxl355_iio_trig_init_param *init_param);

void iio_adxl355_trigger_remove(struct adxl355_iio_trig *trig);

#endif /** IIO_ADXL355_H */
