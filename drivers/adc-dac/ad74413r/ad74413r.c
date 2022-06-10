#include "ad74413r.h"
#include "no_os_crc8.h"
#include "no_os_delay.h"
#include "no_os_error.h"
#include "no_os_util.h"

#define AD74413R_FRAME_SIZE             4
#define AD74413R_CRC_POLYNOMIAL         0x7

#define AD74413R_DIN_DEBOUNCE_LEN       NO_OS_BIT(5)

NO_OS_DECLARE_CRC8_TABLE(_crc_table);

static const unsigned int ad74413r_debounce_map[AD74413R_DIN_DEBOUNCE_LEN] = {
	0,     13,    18,    24,    32,    42,    56,    75,
	100,   130,   180,   240,   320,   420,   560,   750,
	1000,  1300,  1800,  2400,  3200,  4200,  5600,  7500,
	10000, 13000, 18000, 24000, 32000, 42000, 56000, 75000,
};

/** The time required for an ADC conversion (us) by rejection */
static const uint32_t conv_times[] = { 208, 833, 50000, 100000 };

static int ad74413r_rate_to_rejection(uint32_t rate,
				      enum ad74413r_rejection *rejection)
{
	switch (rate) {
	case 20:
		*rejection = AD74413R_REJECTION_50_60;
		return 0;
	case 4800:
		*rejection = AD74413R_REJECTION_NONE;
		return 0;
	case 10:
		*rejection = AD74413R_REJECTION_50_60_HART;
		return 0;
	case 1200:
		*rejection = AD74413R_REJECTION_HART;
		return 0;
	default:
		return -EINVAL;
	}
}

static int ad74413r_rejection_to_rate(uint32_t *rate,
				      enum ad74413r_rejection rejection)
{
	switch (rejection) {
	case AD74413R_REJECTION_50_60:
		*rate = 20;
		return 0;
	case AD74413R_REJECTION_NONE:
		*rate = 4800;
		return 0;
	case AD74413R_REJECTION_50_60_HART:
		*rate = 10;
		return 0;
	case AD74413R_REJECTION_HART:
		*rate = 1200;
		return 0;
	default:
		return -EINVAL;
	}
}

static void ad74413r_format_reg_write(uint8_t reg, uint16_t val, uint8_t *buff)
{
	buff[0] = reg;
	no_os_put_unaligned_be16(val, &buff[1]);
	buff[3] = no_os_crc8(_crc_table, buff, 3, 0);
}

int ad74413r_reg_write(struct ad74413r_desc *desc, uint32_t addr, uint16_t val)
{
	ad74413r_format_reg_write(addr, val, desc->comm_buff);

	return no_os_spi_write_and_read(desc->comm_desc, desc->comm_buff,
					AD74413R_FRAME_SIZE);
}

int ad74413r_reg_update(struct ad74413r_desc *desc, uint32_t addr, int16_t val,
			uint16_t mask)
{
	int ret;
	uint16_t c_val;

	ret = ad74413r_reg_read(desc, addr, &c_val);
	if (ret)
		return ret;

	c_val &= ~mask;
	c_val |= no_os_field_prep(mask, val);

	return ad74413r_reg_write(desc, addr, c_val);
}

int ad74413r_clear_errors(struct ad74413r_desc *desc)
{
	return ad74413r_reg_write(desc, AD74413R_ALERT_STATUS, AD74413R_ERR_CLR_MASK);
}

static int ad74413r_set_info(struct ad74413r_desc *desc, uint8_t val)
{
	return ad74413r_reg_write(desc, AD74413R_READ_SELECT, AD74413R_SPI_RD_RET_INFO_MASK);
}

int ad74413r_reg_read(struct ad74413r_desc *desc, uint32_t addr, uint16_t *val)
{
	int ret;
	uint8_t expected_crc;
	uint8_t b[4] = {0};

	/** Reading a register on AD74413r requires writing the address to the READ_SELECT
	 * register first and then doing another spi read, which will contain the requested
	 * register value.
	 */
	ad74413r_format_reg_write(AD74413R_READ_SELECT, addr, desc->comm_buff);

/*
	struct no_os_spi_msg transfer[2] = {
				{
						.tx_buff = desc->comm_buff,
						.bytes_number = AD74413R_FRAME_SIZE,
						.cs_change = 1
				},
				{
						.rx_buff = b,
						.bytes_number = AD74413R_FRAME_SIZE
				}
		};

	ret = no_os_spi_transfer(desc->comm_desc, transfer, NO_OS_ARRAY_SIZE(transfer));
	if (ret)
		return ret;
*/

	ret = no_os_spi_write_and_read(desc->comm_desc, desc->comm_buff,
				       AD74413R_FRAME_SIZE);
	if (ret)
		return ret;

	ret = no_os_spi_write_and_read(desc->comm_desc, b, AD74413R_FRAME_SIZE);
	if (ret)
		return ret;

	/*
	expected_crc = no_os_crc8(_crc_table, b, 3, 0);
	if (expected_crc != b[3])
		return -EINVAL;
*/
	*val = no_os_get_unaligned_be16(&b[1]);

	return 0;
}

static int ad74413r_check_crc(struct ad74413r_desc *desc)
{
	int ret;
	uint16_t reg_val;

	ret = ad74413r_reg_read(desc, AD74413R_ALERT_STATUS, &reg_val);
	if (ret)
		return ret;

	return no_os_field_get(AD74413R_SPI_CRC_ERR_MASK, reg_val);
}

static int ad74413r_scratch(struct ad74413r_desc *desc)
{
	int ret;
	uint16_t val;

	ret = ad74413r_reg_write(desc, AD74413R_SCRATCH, 0x1234);
	if (ret)
		return ret;

	ret = ad74413r_reg_read(desc, AD74413R_SCRATCH, &val);
	if (ret)
		return ret;

	return 0;
}

int ad74413r_reset(struct ad74413r_desc *desc)
{
	int ret;
	uint16_t status_reg;

	ret = ad74413r_reg_write(desc, AD74413R_CMD_KEY, AD74413R_CMD_KEY_RESET_1);
	if (ret)
		return ret;

	ret = ad74413r_reg_write(desc, AD74413R_CMD_KEY, AD74413R_CMD_KEY_RESET_2);
	if (ret)
		return ret;

	return 0;
}

int ad74413r_set_channel_function(struct ad74413r_desc *desc,
				  enum ad74413r_op_mode ch_func, uint32_t ch)
{

	return ad74413r_reg_update(desc, AD74413R_CH_FUNC_SETUP(ch),
				   ch_func, AD74413R_CH_FUNC_SETUP_MASK);
}

int ad74413r_get_raw_adc_result(struct ad74413r_desc *desc, uint32_t ch,
				int16_t *val)
{
	return ad74413r_reg_read(desc, AD74413R_ADC_RESULT(ch), val);
}

int ad74413r_set_adc_channel_enable(struct ad74413r_desc *desc, uint32_t ch,
				    bool status)
{
	int ret;

	ret = ad74413r_reg_update(desc, AD74413R_ADC_CONV_CTRL,
				  status ? 1 : 0, AD74413R_CH_EN_MASK(ch));
	if (ret)
		return ret;

	desc->channel_configs[ch].enabled = status;

	return 0;
}

int ad74413r_get_adc_range(struct ad74413r_desc *desc, uint32_t ch,
			   uint16_t *val)
{
	int ret;

	ret = ad74413r_reg_read(desc, AD74413R_ADC_CONFIG(ch), val);
	if (ret)
		return ret;

	*val = no_os_field_get(AD74413R_ADC_RANGE_MASK, *val);

	return 0;
}

int ad74413r_set_adc_range(struct ad74413r_desc *desc, uint32_t ch,
			   enum ad74413r_adc_range val)
{
	return ad74413r_reg_update(desc, AD74413R_ADC_CONFIG(ch),
				   val, AD74413R_ADC_RANGE_MASK);
}

int ad74413r_get_adc_rejection(struct ad74413r_desc *desc, uint32_t ch,
			       enum ad74413r_rejection *val)
{
	int ret;

	ret = ad74413r_reg_read(desc, AD74413R_ADC_CONFIG(ch), val);
	if (ret)
		return ret;

	*val = no_os_field_get(AD74413R_ADC_REJECTION_MASK, *val);

	return 0;
}

int ad74413r_set_adc_rejection(struct ad74413r_desc *desc, uint32_t ch,
			       enum ad74413r_rejection val)
{
	return ad74413r_reg_update(desc, AD74413R_ADC_CONFIG(ch),
				   val, AD74413R_ADC_REJECTION_MASK);
}

int ad74413r_get_adc_rate(struct ad74413r_desc *desc, uint32_t ch,
			  uint32_t *val)
{
	int ret;
	enum ad74413r_rejection rejection;

	ret = ad74413r_get_adc_rejection(desc, ch, &rejection);
	if (ret)
		return ret;

	*val = ad74413r_rejection_to_rate(val, rejection);

	return 0;
}

int ad74413r_set_adc_rate(struct ad74413r_desc *desc, uint32_t ch, uint32_t val)
{
	int ret;
	enum ad74413r_rejection rejection;

	ret = ad74413r_rate_to_rejection(val, &rejection);
	if (ret)
		return ret;

	return ad74413r_set_adc_rejection(desc, ch, rejection);
}


int ad74413r_set_adc_conv_seq(struct ad74413r_desc *desc,
			      enum ad74413r_conv_seq status)
{
	int ret;

	ret = ad74413r_reg_update(desc, AD74413R_ADC_CONV_CTRL, status,
				  AD74413R_CONV_SEQ_MASK);
	if (ret)
		return ret;

	/** The write to CONV_SEQ powers up the ADC. If the ADC was powered down, the user must wait
	 * for 100us before the ADC starts making conversions.
	 */
	no_os_udelay(100);

	return 0;
}

int ad74413r_get_adc_single(struct ad74413r_desc *desc, uint32_t ch,
			    uint32_t *val)
{
	int ret;
	uint32_t delay;
	enum ad74413r_rejection rejection;

	ret = ad74413r_set_adc_channel_enable(desc, ch, true);
	if (ret)
		return ret;

	ret = ad74413r_get_adc_rejection(desc, ch, &rejection);
	if (ret)
		return ret;

	ret = ad74413r_set_adc_conv_seq(desc, AD74413R_START_SINGLE);
	if (ret)
		return ret;

	delay = conv_times[rejection];

	/** Wait for this channel to complete the conversion */
	if (delay < 1000)
		no_os_udelay(delay);
	else
		no_os_mdelay(delay);

	ret = ad74413r_get_raw_adc_result(desc, ch, val);
	if (ret)
		return ret;

	ret = ad74413r_set_adc_conv_seq(desc, AD74413R_STOP_PWR_DOWN);
	if (ret)
		return ret;

	return 0;
}

int ad74413r_set_debounce_mode(struct ad74413r_desc *desc, uint32_t ch,
			       uint32_t mode)
{
	return ad74413r_reg_update(desc, AD74413R_DIN_CONFIG(ch),
				   mode, AD74413R_DEBOUNCE_MODE_MASK);
}

int ad74413r_set_debounce_time(struct ad74413r_desc *desc, uint32_t ch,
			       uint32_t time)
{
	uint32_t val = AD74413R_DIN_DEBOUNCE_LEN - 1;

	for (uint32_t i = 0; i < AD74413R_DIN_DEBOUNCE_LEN; i++)
		if (time <= ad74413r_debounce_map[i]) {
			val = i;
			break;
		}

	return ad74413r_reg_update(desc, AD74413R_DIN_CONFIG(ch), val,
				   AD74413R_DEBOUNCE_TIME_MASK);
}

int ad74413r_gpio_get(struct ad74413r_desc *desc, uint32_t ch, uint8_t *val)
{
	int ret;
	uint16_t reg;

	ret = ad74413r_reg_read(desc, AD74413R_DIN_COMP_OUT, &reg);
	if (ret)
		return ret;

	*val = no_os_field_get(NO_OS_BIT(ch), reg);

	return 0;
}

int ad74413r_gpio_set_gpo_config(struct ad74413r_desc *desc, uint32_t ch,
				 uint16_t config)
{
	return ad74413r_reg_update(desc, AD74413R_GPO_CONFIG(ch),
				   config, AD74413R_GPO_CONFIG_SELECT_MASK);
}

int ad74413r_init(struct ad74413r_desc **desc,
		  struct ad74413r_init_param *init_param)
{
	int ret;
	struct ad74413r_desc *descriptor;

	if (!init_param)
		return -EINVAL;

	descriptor = calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;

	ret = no_os_spi_init(&descriptor->comm_desc, &init_param->comm_param);
	if (ret)
		goto comm_err;

	no_os_crc8_populate_msb(_crc_table, AD74413R_CRC_POLYNOMIAL);

	ret = ad74413r_reset(descriptor);
	if (ret)
		return ret;

	uint16_t rev;
	ret = ad74413r_reg_read(descriptor, AD74413R_NOP, &rev);
	if (ret)
		return ret;

	ret = ad74413r_clear_errors(descriptor);
	if (ret)
		return ret;

	ret = ad74413r_scratch(descriptor);

	/** Only for development */
	ret = ad74413r_set_info(descriptor, 1);
	if (ret)
			return ret;

	*desc = descriptor;

	return 0;

comm_err:
	free(descriptor);

	return ret;
}
