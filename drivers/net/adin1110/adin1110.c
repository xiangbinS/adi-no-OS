/***************************************************************************//**
 *   @file   adin1110.c
 *   @brief  Source file of the ADIN1110 driver.
 *   @author Ciprian Regus (ciprian.regus@analog.com)
********************************************************************************
 * Copyright 2023(c) Analog Devices, Inc.
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

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "no_os_spi.h"
#include "adin1110.h"
#include "no_os_util.h"
#include "no_os_delay.h"
#include "no_os_crc8.h";

#define ADIN1110_CRC_POLYNOMIAL	0x7

NO_OS_DECLARE_CRC8_TABLE(_crc_table);

struct _adin1110_priv {
	uint32_t phy_id;
	uint32_t num_ports;
};

static struct _adin1110_priv driver_data[2] = {
	[ADIN1110] = {
		.phy_id= ADIN1110_PHY_ID,
		.num_ports = 1,
	},
	[ADIN2111] = {
		.phy_id = ADIN2111_PHY_ID,
		.num_ports = 2,
	},
};

int adin1110_reg_write(struct adin1110_desc *desc, uint16_t addr, uint32_t data)
{
	struct no_os_spi_msg xfer = {
		.tx_buff = desc->tx_buff,
		.bytes_number = ADIN1110_WR_FRAME_SIZE,
		.cs_change = 1,
	};

	/** The address is 13 bit wide */
	addr &= ADIN1110_ADDR_MASK;
	addr |= ADIN1110_CD_MASK | ADIN1110_RW_MASK;
	no_os_put_unaligned_be16(addr, desc->tx_buff);
	no_os_put_unaligned_be32(data, &desc->tx_buff[2]);

	return no_os_spi_transfer(desc->comm_desc, &xfer, 1);
}

int adin1110_reg_read(struct adin1110_desc *desc, uint16_t addr, uint32_t *data)
{
	struct no_os_spi_msg xfer = {
		.tx_buff = desc->tx_buff,
		.rx_buff = desc->rx_buff,
		.bytes_number = ADIN1110_RD_FRAME_SIZE,
		.cs_change = 1,
		.cs_change_delay = 5,
	};
	int ret;

	no_os_put_unaligned_be16(addr, &desc->tx_buff[0]);
	desc->tx_buff[0] |= NO_OS_BIT(7);
	desc->tx_buff[2] = 0x00;

	ret = no_os_spi_transfer(desc->comm_desc, &xfer, 1);
	if (ret)
		return ret;

	*data = no_os_get_unaligned_be32(&desc->rx_buff[3]);

	return 0;
}

int adin1110_reg_update(struct adin1110_desc *desc, uint16_t addr,
			uint32_t mask, uint32_t data)
{
	uint32_t val;
	int ret;

	ret = adin1110_reg_read(desc, addr, &val);
	if (ret)
		return ret;

	val &= ~mask;
	val |= mask & data;

	return adin1110_reg_write(desc, addr, val);
}

int adin1110_mdio_read(struct adin1110_desc *desc, uint32_t phy_id,
		       uint32_t reg, uint32_t *data)
{
	uint32_t mdio_val = 0;
	uint32_t mdio_done = 0;
	uint32_t ta_err;
	uint32_t val = 0;
	int ret;

	/* Use clause 22 for the MDIO register addressing */
	val |= no_os_field_prep(ADIN1110_MDIO_ST, 0x1);
	val |= no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_RD);
	val |= no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id);
	val |= no_os_field_prep(ADIN1110_MDIO_DEVAD, reg);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(0), val);
	if (ret)
		return ret;

	/* The PHY will set the MDIO_TRDONE bit to 1 once a transaction completes */
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(0), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	*data = no_os_field_get(ADIN1110_MDIO_DATA, mdio_val);

	return 0;
}

int adin1110_mdio_write(struct adin1110_desc *desc, uint32_t phy_id,
			uint32_t reg, uint32_t data)
{
	uint32_t mdio_val = 0;
	uint32_t mdio_done = 0;
	uint32_t val;
	int ret;

	/* Use clause 22 for the MDIO register addressing */
	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x1);
	val |= no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_WR);
	val |= no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id);
	val |= no_os_field_prep(ADIN1110_MDIO_DEVAD, reg);
	val |= no_os_field_prep(ADIN1110_MDIO_DATA, data);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(0), val);
	if (ret)
		return ret;

	/* The PHY will set the MDIO_TRDONE bit to 1 once a transaction completes */
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(0), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	return 0;
}

int adin1110_mdio_write_c45(struct adin1110_desc *desc, uint32_t phy_id,
				   uint32_t dev_id, uint32_t reg, uint32_t data)
{
	uint32_t mdio_val = 0;
	uint32_t mdio_done = 0;
	uint32_t val;
	int ret;

	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x0);
	val |= no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_ADDR);
	val |= no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id);
	val |= no_os_field_prep(ADIN1110_MDIO_DEVAD, dev_id);
	val |= no_os_field_prep(ADIN1110_MDIO_DATA, reg);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(0), val);
	if (ret)
		return ret;

	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x0);
	val |= no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_WR);
	val |= no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id);
	val |= no_os_field_prep(ADIN1110_MDIO_DEVAD, dev_id);
	val |= no_os_field_prep(ADIN1110_MDIO_DATA, data);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(1), val);
	if (ret)
		return ret;

	/* The PHY will set the MDIO_TRDONE bit to 1 once a transaction completes */
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(0), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	mdio_done = 0;
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(1), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	return 0;
}

int adin1110_mdio_read_c45(struct adin1110_desc *desc, uint32_t phy_id,
				  uint32_t dev_id, uint32_t reg, uint32_t *data)
{
	uint32_t mdio_val = 0;
	uint32_t mdio_done = 0;
	uint32_t ta_err;
	uint32_t val;
	int ret;

	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x0);
	val |= no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_ADDR);
	val |= no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id);
	val |= no_os_field_prep(ADIN1110_MDIO_DEVAD, dev_id);
	val |= no_os_field_prep(ADIN1110_MDIO_DATA, reg);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(0), val);
	if (ret)
		return ret;

	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(0), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x0);
	val |= no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_RD);
	val |= no_os_field_prep(ADIN1110_MDIO_DEVAD, dev_id);
	val |= no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(1), val);
	if (ret)
		return ret;

	mdio_done = 0;
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(1), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	*data = no_os_field_get(ADIN1110_MDIO_DATA, mdio_val);

	return 0;
}

static int adin1110_mac_addr_set(struct adin1110_desc *desc, uint8_t mac_address[6])
{
	uint32_t val;
	uint32_t reg_val;
	int ret;

	reg_val = no_os_get_unaligned_be16(&mac_address[0]);

	ret = adin1110_reg_update(desc, ADIN1110_MAC_ADDR_FILTER_UPR_REG,
				  NO_OS_GENMASK(15, 0), reg_val);
	if (ret)
		return ret;

	reg_val = no_os_get_unaligned_be32(&mac_address[2]);

	return adin1110_reg_write(desc, ADIN1110_MAC_ADDR_FILTER_LWR_REG, reg_val);
}

int adin1110_write_fifo(struct adin1110_desc *desc, uint32_t port,
			struct adin1110_eth_buff *eth_buff)
{
	uint32_t field_offset = ADIN1110_WR_HEADER_LEN;
	uint32_t fifo_fsize_reg;
	uint16_t frame_header;
	uint32_t padding = 0;
	uint32_t frame_len;
	uint32_t fifo_reg;
	int ret;

	struct no_os_spi_msg xfer = {
		.tx_buff = desc->tx_buff,
		.cs_change = 1,
	};

	if (port >= driver_data[desc->chip_type].num_ports)
		return -EINVAL;

	if (!port) {
		fifo_reg = ADIN1110_RX_REG;
		fifo_fsize_reg = ADIN1110_RX_FSIZE_REG;
	} else {
		fifo_reg = ADIN2111_RX_P2_REG;
		fifo_fsize_reg = ADIN2111_RX_P2_FSIZE_REG;
	}

	/* 
	 * The minimum frame length is 64 bytes.
	 * The MAC is by default configured to add the frame check sequence, so it's
	 * length shouldn't be added here.
	 */
	if (eth_buff->payload_len + ADIN1110_FCS_LEN < 64)
		padding = 64 - (eth_buff->payload_len + ADIN1110_FCS_LEN);

	frame_len = eth_buff->payload_len + padding + ADIN1110_FRAME_HEADER_LEN;

	/** Align the frame length to 4 bytes */
	frame_len = frame_len + frame_len % 4;

	ret = adin1110_reg_write(desc, fifo_fsize_reg, frame_len - ADIN1110_FRAME_HEADER_LEN);
	if (ret)
		return ret;

	no_os_put_unaligned_be16(fifo_reg, &desc->tx_buff[0]);
	desc->tx_buff[0] |= ADIN1110_SPI_CD | ADIN1110_SPI_RW;

	if (desc->append_crc) {
		desc->tx_buff[2] = no_os_crc8(_crc_table, desc->tx_buff, 2, 0);
		field_offset++;
	}

	/* Set the port on which to send the frame */
	no_os_put_unaligned_be16(port, &desc->tx_buff[field_offset]);
	field_offset += ADIN1110_FRAME_HEADER_LEN;

	xfer.bytes_number = frame_len;

	memcpy(&desc->tx_buff[field_offset], eth_buff->mac_destination, ADIN1110_ETH_ALEN);
	field_offset += ADIN1110_ETH_ALEN;
	memcpy(&desc->tx_buff[field_offset], eth_buff->mac_source, ADIN1110_ETH_ALEN);
	field_offset += ADIN1110_ETH_ALEN;
	memcpy(&desc->tx_buff[field_offset], &eth_buff->ethertype, ADIN1110_ETHERTYPE_LEN);
	field_offset += ADIN1110_ETHERTYPE_LEN;
	memcpy(&desc->tx_buff[field_offset], eth_buff->payload, eth_buff->payload_len + padding);
	
	return no_os_spi_transfer(desc->comm_desc, &xfer, 1);
}

int adin1110_read_fifo(struct adin1110_desc *desc, uint32_t port, struct adin1110_eth_buff *eth_buff)
{

	uint32_t field_offset = ADIN1110_RD_HEADER_LEN;
	uint32_t frame_size_no_fcs;
	uint32_t fifo_fsize_reg;
	uint32_t frame_content;
	uint32_t frame_size;
	uint32_t count = 0;
	uint32_t fifo_reg;
	uint32_t rem;

	struct no_os_spi_msg xfer = {
		.tx_buff = desc->tx_buff,
		.rx_buff = desc->rx_buff,
		.cs_change = 1,
	};
	int ret;

	if (port >= driver_data[desc->chip_type].num_ports)
		return -EINVAL;

	if (!port) {
		fifo_reg = ADIN1110_RX_REG;
		fifo_fsize_reg = ADIN1110_RX_FSIZE_REG;
	} else {
		fifo_reg = ADIN2111_RX_P2_REG;
		fifo_fsize_reg = ADIN2111_RX_P2_FSIZE_REG;
	}

	ret = adin1110_reg_read(desc, fifo_fsize_reg, &frame_size);
	if (ret)
		return ret;

	if (frame_size < ADIN1110_FRAME_HEADER_LEN + ADIN1110_FEC_LEN)
		return ret;

	no_os_put_unaligned_be16(fifo_reg, &desc->tx_buff[0]);
	desc->tx_buff[0] |= ADIN1110_SPI_CD;
	desc->tx_buff[2] = 0x00;

	/* Can only read multiples of 4 bytes (the last bytes might be 0) */
	xfer.bytes_number = frame_size + frame_size % 4;

	if (desc->append_crc) {
		desc->tx_buff[2] = no_os_crc8(_crc_table, desc->tx_buff, 2, 0);
		field_offset ++;
	}

	/* Set the port from which to receive the frame */
	no_os_put_unaligned_be16(port, &desc->tx_buff[field_offset]);
	field_offset += ADIN1110_FRAME_HEADER_LEN;

	/** Burst read the whole frame */
	ret = no_os_spi_transfer(desc->comm_desc, &xfer, 1);
	if (ret)
		return ret;

	memcpy(eth_buff->mac_destination, &desc->rx_buff[field_offset], ADIN1110_ETH_ALEN);
	field_offset += ADIN1110_ETH_ALEN;
	memcpy(eth_buff->mac_destination, &desc->rx_buff[field_offset], ADIN1110_ETH_ALEN);
	field_offset += ADIN1110_ETH_ALEN;
	memcpy(&eth_buff->ethertype, &desc->rx_buff[field_offset], ADIN1110_ETHERTYPE_LEN);
	field_offset += ADIN1110_ETHERTYPE_LEN;
	memcpy(eth_buff->payload, &desc->rx_buff[field_offset], frame_size);

	return 0;
}

int adin1110_add_mac_filter(struct adin1110_desc *desc, uint32_t port, uint8_t *addr)
{
	uint32_t port_rules_mask;
	uint32_t port_rules;
	uint32_t offset;
	uint32_t val;
	int ret;

	if (!port) {
		port_rules = ADIN1110_MAC_P1_ADDR_SLOT;
		port_rules_mask = ADIN1110_MAC_ADDR_APPLY2PORT;
	} else {
		port_rules = ADIN2110_MAC_P2_ADDR_SLOT;
		port_rules_mask = ADIN2111_MAC_ADDR_APPLY2PORT2;
	}

	if (port_rules & port_rules_mask)
		port_rules_mask |= ADIN1110_MAC_ADDR_TO_HOST | ADIN2111_MAC_ADDR_TO_OTHER_PORT;		

	port_rules_mask |= NO_OS_GENMASK(15, 0);
	val = port_rules | no_os_get_unaligned_be16(addr);
	ret = adin1110_reg_update(desc, ADIN1110_MAC_ADDR_FILTER_UPR_REG + offset,
				  port_rules_mask, val);
	if (ret)
		return ret;
	
	val = no_os_get_unaligned_be32(&addr[2]);
	ret = adin1110_reg_write(desc, ADIN1110_MAC_ADDR_FILTER_LWR_REG, val);
	if (ret)
		return ret;

	return ret;
}

static int adin1110_mac_reset(struct adin1110_desc *desc)
{
	uint32_t val;
	int ret;

	ret = adin1110_reg_write(desc, ADIN1110_SOFT_RST_REG, ADIN1110_SWRESET_KEY1);
	if (ret)
		return ret;

	ret = adin1110_reg_write(desc, ADIN1110_SOFT_RST_REG, ADIN1110_SWRESET_KEY2);
	if (ret)
		return ret;

	ret = adin1110_reg_write(desc, ADIN1110_SOFT_RST_REG, ADIN1110_SWRELEASE_KEY1);
	if (ret)
		return ret;

	ret = adin1110_reg_write(desc, ADIN1110_SOFT_RST_REG, ADIN1110_SWRELEASE_KEY2);
	if (ret)
		return ret;

	ret = adin1110_reg_read(desc, ADIN1110_MAC_RST_STATUS_REG, &val);
	if (ret)
		return ret;

	if (!val)
		return -EBUSY;

	return 0;
}

static int adin1110_check_reset(struct adin1110_desc *desc)
{
	uint32_t reg_val;
	int ret;

	ret = adin1110_reg_read(desc, ADIN1110_STATUS0_REG, &reg_val);
	if (ret)
		return ret;

	reg_val = no_os_field_get(ADIN1110_RESETC_MASK, reg_val);
	if (!reg_val)
		return -EBUSY;

	ret = adin1110_reg_read(desc, ADIN1110_CONFIG1_REG, &reg_val);
	if (ret)
		return ret;

	return adin1110_reg_update(desc, ADIN1110_CONFIG1_REG,
			    	   ADIN1110_CONFIG1_SYNC, ADIN1110_CONFIG1_SYNC);

}

static int adin1110_phy_reset(struct adin1110_desc *desc)
{
	uint32_t phy_id;
	uint32_t expected_id;
	int ret;

	/* The timing values for the reset sequence are spcified in the datasheet */
	ret = no_os_gpio_set_value(desc->reset_gpio, NO_OS_GPIO_LOW);
	if (ret)
		return ret;

	no_os_mdelay(10);

	ret = no_os_gpio_set_value(desc->reset_gpio, NO_OS_GPIO_HIGH);
	if (ret)
		return ret;

	no_os_mdelay(90);

	ret = adin1110_reg_read(desc, ADIN1110_PHY_ID_REG, &phy_id);
	if (ret)
		return ret;

	expected_id = driver_data[desc->chip_type].phy_id;

	if (phy_id != expected_id)
		return -EINVAL;

	return 0;
}

int adin1110_sw_reset(struct adin1110_desc *desc)
{
	return adin1110_reg_write(desc, ADIN1110_RESET_REG, 0x1);
}

int adin1110_set_promisc(struct adin1110_desc *desc, uint32_t port, bool promisc)
{
	uint32_t num_ports;
	uint32_t fwd_mask;
	int ret;

	if (port >= driver_data[desc->chip_type].num_ports)
		return -EINVAL;

	if (!port)
		fwd_mask = ADIN1110_FWD_UNK2HOST_MASK;
	else
		fwd_mask = ADIN2111_P2_FWD_UNK2HOST_MASK;

	return adin1110_reg_update(desc, ADIN1110_CONFIG2_REG, fwd_mask,
				   promisc ? fwd_mask : 0);
}

static int adin1110_setup_phy(struct adin1110_desc *desc)
{
	uint32_t reg_val;
	uint32_t ports;
	uint32_t sw_pd;
	size_t i;
	int ret;
	uint32_t retries = 0;

	ports = driver_data[desc->chip_type].num_ports;

	for (i = 0; i < ports; i++) {
		ret = adin1110_mdio_read(desc, ADIN1110_MDIO_PHY_ID(i),
					 ADIN1110_MI_CONTROL_REG, &reg_val);
		if (ret)
			return ret;

		/* Get the PHY out of software power down to start the autonegotiation process*/
		sw_pd = no_os_field_get(ADIN1110_MI_SFT_PD_MASK, reg_val);
		if (sw_pd) {
			while (reg_val & ADIN1110_MI_SFT_PD_MASK) {
				reg_val &= ~ADIN1110_MI_SFT_PD_MASK;
				ret = adin1110_mdio_write(desc, ADIN1110_MDIO_PHY_ID(i),
							ADIN1110_MI_CONTROL_REG, reg_val);
				if (ret)
					return ret;
				ret = adin1110_mdio_read(desc, ADIN1110_MDIO_PHY_ID(i),
							 ADIN1110_MI_CONTROL_REG, &reg_val);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

static int adin1110_setup_mac(struct adin1110_desc *desc)
{
	int ret;
	uint32_t reg_val;

	ret = adin1110_reg_update(desc, ADIN1110_CONFIG2_REG, ADIN1110_CRC_APPEND,
				  ADIN1110_CRC_APPEND);
	if (ret)
		return ret;

	ret = adin1110_reg_read(desc, ADIN1110_CONFIG2_REG, &reg_val);
	if (ret)
		return ret;

	reg_val = ADIN1110_TX_RDY_IRQ | ADIN1110_RX_RDY_IRQ | ADIN1110_SPI_ERR_IRQ;
	if (desc->chip_type == ADIN2111)
		reg_val |= ADIN2111_RX_RDY_IRQ;

	ret = adin1110_reg_write(desc, ADIN1110_IMASK1_REG, reg_val);

	return 0;
}

int adin1110_init(struct adin1110_desc **desc,
                  struct adin1110_init_param *param)
{
	struct adin1110_desc *descriptor;
	uint32_t reg_val;
	uint32_t phy_id;
	size_t i;
	int ret;

	descriptor = calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;

	ret = no_os_spi_init(&descriptor->comm_desc, &param->comm_param);
	if (ret)
		goto err;

	no_os_crc8_populate_msb(_crc_table, ADIN1110_CRC_POLYNOMIAL);
	strncpy(descriptor->mac_address, param->mac_address, ADIN1110_MAC_LEN);

	ret = no_os_gpio_get(&descriptor->reset_gpio, &param->reset_param);
	if (ret)
		goto err;

	ret = no_os_gpio_direction_output(descriptor->reset_gpio, NO_OS_GPIO_OUT);
	if (ret)
		return ret;

	descriptor->chip_type = param->chip_type;

	ret = adin1110_sw_reset(descriptor);
	if (ret)
		return ret;

	ret = no_os_gpio_set_value(descriptor->reset_gpio, NO_OS_GPIO_HIGH);
	if (ret)
		return ret;

	ret = adin1110_setup_mac(descriptor);
	if (ret)
		return ret;

	ret = adin1110_setup_phy(descriptor);
	if (ret)
		goto err;

	ret = adin1110_check_reset(descriptor);
	if (ret)
		goto err;

	*desc = descriptor;

	return 0;

err:
	free(descriptor);

	return ret;
}

int adin1110_remove(struct adin1110_desc *desc)
{
	int ret;

	if (!desc)
		return -EINVAL;

	ret = no_os_spi_remove(desc->comm_desc);
	if (ret)
		return ret;

	ret = no_os_spi_remove(desc->reset_gpio);
	if (ret)
		return ret;

	free(desc);

	return 0;
}