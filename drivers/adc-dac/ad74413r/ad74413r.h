#ifndef _AD74413R_H
#define _AD74413R_H

#include "stdint.h"
#include "stdbool.h"
#include "no_os_spi.h"

#define AD74413R_N_CHANNELS             4

#define AD74413R_CH_A                   0
#define AD74413R_CH_B                   1
#define AD74413R_CH_C                   2
#define AD74413R_CH_D                   3

/** The value of the sense resistor in ohms */
#define AD74413R_RSENSE                 100

/** Register map */

#define AD74413R_NOP                            0x00
#define AD74413R_CH_FUNC_SETUP(x)               (0x01 + x)
#define AD74413R_ADC_CONFIG(x)                  (0x05 + x)
#define AD74413R_DIN_CONFIG(x)                  (0x09 + x)
#define AD74413R_GPO_PARALLEL                   0x0D
#define AD74413R_GPO_CONFIG(x)                  (0x0E + x)
#define AD74413R_OUTPUT_CONFIG(x)               (0x12 + x)
#define AD74413R_DAC_CODE(x)                    (0x16 + x)
#define AD74413R_DAC_CLR_CODE(x)                (0x1A + x)
#define AD74413R_DAC_ACTIVE(x)                  (0x1E + x)
#define AD74413R_DIN_THRESH                     0x22
#define AD74413R_ADC_CONV_CTRL                  0x23
#define AD74413R_DIAG_ASSIGN                    0x24
#define AD74413R_DIN_COMP_OUT                   0x25
#define AD74413R_ADC_RESULT(x)                  (0x26 + x)
#define AD74413R_DIAG_RESULT(x)                 (0x2A + x)
#define AD74413R_ALERT_STATUS                   0x2E
#define AD74413R_LIVE_STATUS                    0x2F
#define AD74413R_ALERT_MASK                     0x3C
#define AD74413R_DIN_COUNTER(x)                 (0x3D + x)
#define AD74413R_READ_SELECT                    0x41
#define AD74413R_THERM_RST                      0x43
#define AD74413R_CMD_KEY                        0x44
#define AD74413R_SCRATCH                        0x45
#define AD74413R_SILICON_REV                    0x46

#define AD74413R_ALERT_STATUS_RESET             0xFF

/** Software reset sequence */
#define AD74413R_CMD_KEY_RESET_1                0x15FA
#define AD74413R_CMD_KEY_RESET_2                0xAF51

#define AD74413R_CH_FUNC_SETUP_MASK             NO_OS_GENMASK(3, 0)
#define AD74413R_CH_EN_MASK(x)                  NO_OS_BIT(x)
#define AD74413R_ADC_RANGE_MASK                 NO_OS_GENMASK(7, 5)
#define AD74413R_ADC_REJECTION_MASK             NO_OS_GENMASK(4, 3)
#define AD74413R_CONV_SEQ_MASK                  NO_OS_GENMASK(9, 8)
#define AD74413R_DEBOUNCE_TIME_MASK             NO_OS_GENMASK(4, 0)
#define AD74413R_DEBOUNCE_MODE_MASK             NO_OS_BIT(5)
#define AD74413R_GPO_CONFIG_SELECT_MASK         NO_OS_GENMASK(2, 0)

enum ad74413r_rejection {
	AD74413R_REJECTION_50_60,
	AD74413R_REJECTION_NONE,
	AD74413R_REJECTION_50_60_HART,
	AD74413R_REJECTION_HART
};

enum ad74413r_op_mode {
	AD74413R_HIGH_Z,
	AD74413R_VOLTAGE_OUT,
	AD74413R_CURRENT_OUT,
	AD74413R_VOLTAGE_IN,
	AD74413R_CURRENT_IN_EXT,
	AD74413R_CURRENT_IN_LOOP,
	AD74413R_RESISTANCE,
	AD74413R_DIGITAL_INPUT,
	AD74413R_DIGITAL_INPUT_LOOP,
	AD74413R_CURRENT_IN_EXT_HART,
	AD74413R_CURRENT_IN_LOOP_HART,
};

enum ad74413r_adc_range {
	AD74413R_ADC_RANGE_10V,
	AD74413R_ADC_RANGE_2P5V_EXT_POW,
	AD74413R_ADC_RANGE_2P5V_INT_POW,
	AD74413R_ADC_RANGE_5V_BI_DIR
};

enum ad74413r_conv_seq {
	AD74413R_STOP_PWR_UP,
	AD74413R_START_SINGLE,
	AD74413R_START_CONT,
	AD74413R_STOP_PWR_DOWN,
};

struct ad74413r_init_param {
	struct no_os_spi_init_param comm_param
	};

struct ad74413r_decimal {
	int32_t integer;
	uint32_t decimal;
};

struct ad74413r_channel_config {
	bool enabled;
	enum ad74413r_op_mode function;
};

struct ad74413r_desc {
	struct no_os_spi_desc *comm_desc;
	uint8_t tx_comm_buff[4];
	struct ad74413r_channel_config channel_configs[AD74413R_N_CHANNELS];
};

#endif _AD74413R_H // _AD74413R_H
