/***************************************************************************//**
 *   @file   headless.c
 *   @brief  adrv9002 main project file.
 *   @author Darius Berghe (darius.berghe@analog.com)
********************************************************************************
 * Copyright 2020(c) Analog Devices, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XILINX_PLATFORM
#include "xil_cache.h"
#endif /* XILINX_PLATFORM */

#include "no-os/error.h"
#include "no-os/util.h"
#include "no-os/spi.h"

#include "axi_adc_core.h"
#include "axi_dac_core.h"
#include "axi_dmac.h"

#include "parameters.h"

#ifdef IIO_SUPPORT
#include "iio_app.h"
#include "iio_axi_adc.h"
#include "iio_axi_dac.h"
#endif

#include "adrv9002.h"
#include "adi_adrv9001.h"
#include "adi_adrv9001_arm.h"
#include "adi_adrv9001_radio.h"

int get_sampling_frequency(struct axi_adc *dev, uint32_t chan,
			   uint64_t *sampling_freq_hz)
{
	if (!dev || !sampling_freq_hz)
		return -EINVAL;

	*sampling_freq_hz =
		adrv9002_init_get(dev)->rx.rxChannelCfg[chan].profile.rxOutputRate_Hz;
	return SUCCESS;
}

#ifdef IIO_SUPPORT

static int32_t iio_run(struct iio_axi_adc_init_param *adc_pars,
		       struct iio_axi_dac_init_param *dac_pars)
{
	char *names[] = {
		"axi_adc1",
		"axi_dac1",
		"axi_adc2",
		"axi_dac2"
	};
	struct iio_axi_adc_desc *adcs[IIO_DEV_COUNT];
	struct iio_axi_dac_desc *dacs[IIO_DEV_COUNT];
	struct iio_data_buffer iio_dac_buffers[IIO_DEV_COUNT];
	struct iio_data_buffer iio_adc_buffers[IIO_DEV_COUNT];
	struct iio_device *iio_descs[IIO_DEV_COUNT * 2];
	struct iio_app_device app_devices[IIO_DEV_COUNT * 2] = {0};
	int32_t i, ret;

	for (i = 0; i < IIO_DEV_COUNT; i++) {
		/* ADC setup */
		iio_adc_buffers[i].buff = adc_buffers[i];
		iio_adc_buffers[i].size = sizeof(adc_buffers[i]);
		ret = iio_axi_adc_init(&adcs[i], &adc_pars[i]);
		if (ret < 0)
			return ret;
		iio_axi_adc_get_dev_descriptor(adcs[i], &iio_descs[i]);
		app_devices[i].name = names[i];
		app_devices[i].dev = adcs[i];
		app_devices[i].dev_descriptor = iio_descs[i];
		app_devices[i].read_buff = &iio_adc_buffers[i];

		/* DAC setup */
		iio_dac_buffers[i].buff = dac_buffers[i];
		iio_dac_buffers[i].size = sizeof(dac_buffers[i]);
		ret = iio_axi_dac_init(&dacs[i], &dac_pars[i]);
		if (ret < 0)
			return ret;
		iio_axi_dac_get_dev_descriptor(dacs[i + 1], &iio_descs[i + 1]);
		app_devices[i + 1].name = names[i];
		app_devices[i + 1].dev = dacs[i];
		app_devices[i + 1].dev_descriptor = iio_descs[i + 1];
		app_devices[i + 1].write_buff = &iio_dac_buffers[i];
	}

	return iio_app_run(app_devices, sizeof(app_devices));
}
#endif

int main(void)
{
	int ret;
	struct adi_common_ApiVersion api_version;
	struct adi_adrv9001_ArmVersion arm_version;
	struct adi_adrv9001_SiliconVersion silicon_version;
	struct adrv9002_rf_phy phy;
	struct adi_adrv9001_GainControlCfg *agc_settings;
	unsigned int c;

	struct axi_adc_init rx1_adc_init = {
		"axi-adrv9002-rx-lpc",
		RX1_ADC_BASEADDR,
#ifndef ADRV9002_RX2TX2
		ADRV9001_NUM_SUBCHANNELS,
#else
		ADRV9001_NUM_CHANNELS,
#endif
	};

	struct axi_dac_channel  tx1_dac_channels[2];
	tx1_dac_channels[0].sel = AXI_DAC_DATA_SEL_DMA;
	tx1_dac_channels[1].sel = AXI_DAC_DATA_SEL_DMA;

	struct axi_dac_init tx1_dac_init = {
		"axi-adrv9002-tx-lpc",
		TX1_DAC_BASEADDR,
#ifndef ADRV9002_RX2TX2
		ADRV9001_NUM_SUBCHANNELS,
#else
		ADRV9001_NUM_CHANNELS,
#endif
		tx1_dac_channels,
	};

#ifndef ADRV9002_RX2TX2
	struct axi_adc_init rx2_adc_init = {
		"axi-adrv9002-rx2-lpc",
		RX2_ADC_BASEADDR,
		ADRV9001_NUM_SUBCHANNELS,
	};

	struct axi_dac_channel  tx2_dac_channels[2];
	tx2_dac_channels[0].sel = AXI_DAC_DATA_SEL_DMA;
	tx2_dac_channels[1].sel = AXI_DAC_DATA_SEL_DMA;

	struct axi_dac_init tx2_dac_init = {
		"axi-adrv9002-tx2-lpc",
		TX2_DAC_BASEADDR,
		ADRV9001_NUM_SUBCHANNELS,
		tx2_dac_channels,
	};
#endif
	struct axi_dmac_init rx1_dmac_init = {
		"rx_dmac",
		RX1_DMA_BASEADDR,
		DMA_DEV_TO_MEM,
		0
	};

	struct axi_dmac_init tx1_dmac_init = {
		"tx_dmac",
		TX1_DMA_BASEADDR,
		DMA_MEM_TO_DEV,
		DMA_CYCLIC,
	};

#ifndef ADRV9002_RX2TX2
	struct axi_dmac_init rx2_dmac_init = {
		"rx_dmac",
		RX2_DMA_BASEADDR,
		DMA_DEV_TO_MEM,
		0
	};

	struct axi_dmac_init tx2_dmac_init = {
		"tx_dmac",
		TX2_DMA_BASEADDR,
		DMA_MEM_TO_DEV,
		DMA_CYCLIC,
	};
#endif

#ifdef XILINX_PLATFORM
	Xil_ICacheEnable();
	Xil_DCacheEnable();
#endif /* XILINX_PLATFORM */

	printf("Hello\n");

	memset(&phy, 0, sizeof(struct adrv9002_rf_phy));

#if defined(ADRV9002_RX2TX2)
	phy.rx2tx2 = true;
#endif

	/* Initialize AGC */
	agc_settings = adrv9002_agc_settings_get();
	for (c = 0; c < ADRV9002_CHANN_MAX; c++) {
		phy.rx_channels[c].agc = *agc_settings;
	}

	ret = adrv9002_setup(&phy, adrv9002_init_get(phy.rx1_adc));
	if (ret)
		return ret;

	adi_adrv9001_ApiVersion_Get(phy.adrv9001, &api_version);
	adi_adrv9001_arm_Version(phy.adrv9001, &arm_version);
	adi_adrv9001_SiliconVersion_Get(phy.adrv9001, &silicon_version);

	printf("%s Rev %d.%d, Firmware %u.%u.%u.%u API version: %u.%u.%u successfully initialized\n",
	       "ADRV9002", silicon_version.major, silicon_version.minor,
	       arm_version.majorVer, arm_version.minorVer,
	       arm_version.maintVer, arm_version.rcVer, api_version.major,
	       api_version.minor, api_version.patch);

	/* Initialize the ADC/DAC cores */
	ret = axi_adc_init(&phy.rx1_adc, &rx1_adc_init);
	if (ret) {
		printf("axi_adc_init() failed with status %d\n", ret);
		goto error;
	}

	ret = axi_dac_init(&phy.tx1_dac, &tx1_dac_init);
	if (ret) {
		printf("axi_dac_init() failed with status %d\n", ret);
		goto error;
	}
	phy.tx1_dac->clock_hz = adrv9002_init_get(phy.rx1_adc)->tx.txProfile[0].txInputRate_Hz;
#ifndef ADRV9002_RX2TX2
	ret = axi_adc_init(&phy.rx2_adc, &rx2_adc_init);
	if (ret) {
		printf("axi_adc_init() failed with status %d\n", ret);
		goto error;
	}

	ret = axi_dac_init(&phy.tx2_dac, &tx2_dac_init);
	if (ret) {
		printf("axi_dac_init() failed with status %d\n", ret);
		goto error;
	}
	phy.tx2_dac->clock_hz = adrv9002_init_get(phy.rx1_adc)->tx.txProfile[1].txInputRate_Hz;
#endif

	/* Post AXI DAC/ADC setup, digital interface tuning */
	ret = adrv9002_post_setup(&phy);
	if (ret) {
		printf("adrv9002_post_setup() failed with status %d\n", ret);
		goto error;
	}

	/* TODO: Remove this when it gets fixed in the API. */
	adi_adrv9001_Radio_Channel_ToState(phy.adrv9001, ADI_RX,
					   ADI_CHANNEL_1, ADI_ADRV9001_CHANNEL_PRIMED);
	adi_adrv9001_Radio_Channel_ToState(phy.adrv9001, ADI_RX,
					   ADI_CHANNEL_1, ADI_ADRV9001_CHANNEL_RF_ENABLED);
	adi_adrv9001_Radio_Channel_ToState(phy.adrv9001, ADI_RX,
					   ADI_CHANNEL_2, ADI_ADRV9001_CHANNEL_PRIMED);
	adi_adrv9001_Radio_Channel_ToState(phy.adrv9001, ADI_RX,
					   ADI_CHANNEL_2, ADI_ADRV9001_CHANNEL_RF_ENABLED);

	/* Initialize the AXI DMA Controller cores */
	ret = axi_dmac_init(&phy.tx1_dmac, &tx1_dmac_init);
	if (ret) {
		printf("axi_dmac_init() failed with status %d\n", ret);
		goto error;
	}

	ret = axi_dmac_init(&phy.rx1_dmac, &rx1_dmac_init);
	if (ret) {
		printf("axi_dmac_init() failed with status %d\n", ret);
		goto error;
	}
#ifndef ADRV9002_RX2TX2
	ret = axi_dmac_init(&phy.tx2_dmac, &tx2_dmac_init);
	if (ret) {
		printf("axi_dmac_init() failed with status %d\n", ret);
		goto error;
	}

	ret = axi_dmac_init(&phy.rx2_dmac, &rx2_dmac_init);
	if (ret) {
		printf("axi_dmac_init() failed with status %d\n", ret);
		goto error;
	}
#endif

#ifdef DAC_DMA_EXAMPLE
	axi_dac_load_custom_data(phy.tx1_dac, sine_lut_iq,
				 ARRAY_SIZE(sine_lut_iq),
				 (uintptr_t)dac_buffers[0]);
#ifndef ADRV9002_RX2TX2
	axi_dac_load_custom_data(phy.tx2_dac, sine_lut_iq,
				 ARRAY_SIZE(sine_lut_iq),
				 (uintptr_t)dac_buffers[1]);
#endif
#ifdef XILINX_PLATFORM
	Xil_DCacheFlush();
#endif /* XILINX_PLATFORM */

	axi_dmac_transfer(phy.tx1_dmac, (uintptr_t)dac_buffers[0], sizeof(sine_lut_iq));
#ifndef ADRV9002_RX2TX2
	axi_dmac_transfer(phy.tx2_dmac, (uintptr_t)dac_buffers[1], sizeof(sine_lut_iq));
#endif

	mdelay(1000);

	/* Transfer 16384 samples from ADC to MEM */
	axi_dmac_transfer(phy.rx1_dmac,
			  (uintptr_t)adc_buffers[0],
			  16384 * /* nr of samples */
#ifndef ADRV9002_RX2TX2
			  ADRV9001_NUM_SUBCHANNELS * /* rx1 i/q */
#else
			  ADRV9001_NUM_CHANNELS * /* rx1 i/q, rx2 i/q*/
#endif
			  2 /* bytes per sample */);
#ifdef XILINX_PLATFORM
	Xil_DCacheInvalidateRange((uintptr_t)adc_buffers[0],
				  16384 * /* nr of samples */
#ifndef ADRV9002_RX2TX2
				  ADRV9001_NUM_SUBCHANNELS * /* rx1 i/q */
#else
				  ADRV9001_NUM_CHANNELS * /* rx1 i/q, rx2 i/q*/
#endif
				  2 /* bytes per sample */);
#endif /* XILINX_PLATFORM */
#ifndef ADRV9002_RX2TX2
	axi_dmac_transfer(phy.rx2_dmac,
			  (uintptr_t)adc_buffers[1],
			  16384 * /* nr of samples */
			  ADRV9001_NUM_SUBCHANNELS * /* nr of channels */
			  2 /* bytes per sample */);
#ifdef XILINX_PLATFORM
	Xil_DCacheInvalidateRange((uintptr_t)adc_buffers[1],
				  16384 * /* nr of samples */
				  ADRV9001_NUM_SUBCHANNELS * /* nr of channels */
				  2 /* bytes per sample */);
#endif /* XILINX_PLATFORM */
	printf("DAC_DMA_EXAMPLE: address=%#lx samples=%lu channels=%u bits=%lu\n",
	       (uintptr_t)adc_buffers[1], 16384 * rx2_adc_init.num_channels,
	       rx2_adc_init.num_channels, 8 * sizeof(adc_buffers[1][0]));
#endif
	printf("DAC_DMA_EXAMPLE: address=%#lx samples=%lu channels=%u bits=%lu\n",
	       (uintptr_t)adc_buffers[0], 16384 * rx1_adc_init.num_channels,
	       rx1_adc_init.num_channels, 8 * sizeof(adc_buffers[0][0]));
#endif

#ifdef IIO_SUPPORT
	struct iio_axi_adc_init_param iio_axi_adcs_init_par[] = {{
			.rx_adc = phy.rx1_adc,
			.rx_dmac = phy.rx1_dmac,
#ifdef XILINX_PLATFORM
			.dcache_invalidate_range = (void (*)(uint32_t, uint32_t))Xil_DCacheInvalidateRange,
#endif /* XILINX_PLATFORM */
			.get_sampling_frequency = get_sampling_frequency,
		},
#ifndef ADRV9002_RX2TX2
		{
			.rx_adc = phy.rx2_adc,
			.rx_dmac = phy.rx2_dmac,
#ifdef XILINX_PLATFORM
			.dcache_invalidate_range = (void (*)(uint32_t, uint32_t))Xil_DCacheInvalidateRange,
#endif /* XILINX_PLATFORM */
			.get_sampling_frequency = get_sampling_frequency,
		}
#endif
	};

	struct iio_axi_dac_init_param iio_axi_dacs_init_par[] = {{
			.tx_dac = phy.tx1_dac,
			.tx_dmac = phy.tx1_dmac,
#ifdef XILINX_PLATFORM
			.dcache_flush_range = (void (*)(uint32_t, uint32_t))Xil_DCacheFlushRange,
#endif /* XILINX_PLATFORM */
		},
#ifndef ADRV9002_RX2TX2
		{
			.tx_dac = phy.tx2_dac,
			.tx_dmac = phy.tx2_dmac,
#ifdef XILINX_PLATFORM
			.dcache_flush_range = (void (*)(uint32_t, uint32_t))Xil_DCacheFlushRange,
#endif /* XILINX_PLATFORM */
		}
#endif
	};

	ret = iio_run(iio_axi_adcs_init_par, iio_axi_dacs_init_par);
	if (ret < 0) {
		printf("iio_run() failed with status %d\n", ret);
		goto error;
	}
#endif
	printf("Bye\n");

error:
	adi_adrv9001_HwClose(phy.adrv9001);
	axi_adc_remove(phy.rx1_adc);
	axi_dac_remove(phy.tx1_dac);
	axi_adc_remove(phy.rx2_adc);
	axi_dac_remove(phy.tx2_dac);
	axi_dmac_remove(phy.rx1_dmac);
	axi_dmac_remove(phy.tx1_dmac);
	axi_dmac_remove(phy.rx2_dmac);
	axi_dmac_remove(phy.tx2_dmac);
	return ret;
}
