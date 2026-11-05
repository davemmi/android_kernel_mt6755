/*
 * Meizu M5 Note (M1621) panel: Renesas R63350, TCL/Sharp glass, FHD video mode.
 *
 * Reconstructed from the Flyme 8 stock kernel (3.18.35, 2020-07-09):
 *   - LCM_PARAMS from lcm_get_params()
 *   - init table (38 entries) from lcm_init()/push_table()
 *   - power and sleep sequences from lcm_init_power()/lcm_suspend()
 * Board wiring (stock DT): RST = GPIO158, bias ENP = GPIO12, ENN = GPIO7,
 * TPS65132 bias regulator on I2C0 @ 0x3e (same as the M3 Note M681).
 */
#define LOG_TAG "LCM"

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/of.h>

#include "lcm_drv.h"


#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"][r63350] "fmt, ##args)

#define FRAME_WIDTH   1080
#define FRAME_HEIGHT  1920

#define M1621_BIAS_ENP_PIN  12
#define M1621_BIAS_ENN_PIN  7

#define REGFLAG_UDELAY        0xFFFB
#define REGFLAG_DELAY         0xFFFC
#define REGFLAG_END_OF_TABLE  0xFFFD

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)  (lcm_util.set_reset_pin((v)))
#define MDELAY(n)         (lcm_util.mdelay(n))
#define UDELAY(n)         (lcm_util.udelay(n))
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)

/* ------------------------------------------------------------------ */
/* TPS65132 bias regulator (I2C0, 0x3e)                               */
/* ------------------------------------------------------------------ */
#define TPS_I2C_BUSNUM 0
#define TPS_ADDR       0x3E
#define TPS_NAME       "tps65132"

static struct i2c_client *tps65132_i2c_client;

static const struct of_device_id tps65132_of_match[] = {
	{ .compatible = "mediatek,i2c_lcd_bias" },
	{},
};

static const struct i2c_device_id tps65132_id[] = {
	{ TPS_NAME, 0 },
	{}
};

static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	LCM_LOGI("tps65132 probe addr=0x%02x\n", client->addr);
	tps65132_i2c_client = client;
	return 0;
}

static int tps65132_remove(struct i2c_client *client)
{
	if (tps65132_i2c_client == client)
		tps65132_i2c_client = NULL;
	return 0;
}

static struct i2c_driver tps65132_iic_driver = {
	.id_table = tps65132_id,
	.probe = tps65132_probe,
	.remove = tps65132_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = TPS_NAME,
		.of_match_table = tps65132_of_match,
	},
};

static int tps65132_write_bytes(unsigned char addr, unsigned char value)
{
	char buf[2] = { addr, value };
	int ret, i;

	if (!tps65132_i2c_client) {
		LCM_LOGI("tps65132 client not ready\n");
		return -ENODEV;
	}
	for (i = 0; i < 5; i++) {
		ret = i2c_master_send(tps65132_i2c_client, buf, 2);
		if (ret >= 0)
			return ret;
		MDELAY(5);
	}
	LCM_LOGI("tps65132 write reg 0x%02x failed (%d)\n", addr, ret);
	return ret;
}

static int __init tps65132_iic_init(void)
{
	/* The stock DT already declares i2c_lcd_bias@3e on I2C0; the
	 * of_match_table above binds to it. No board_info needed. */
	return i2c_add_driver(&tps65132_iic_driver);
}

static void __exit tps65132_iic_exit(void)
{
	i2c_del_driver(&tps65132_iic_driver);
}

module_init(tps65132_iic_init);
module_exit(tps65132_iic_exit);

/* ------------------------------------------------------------------ */
/* Bias enable pads                                                   */
/* ------------------------------------------------------------------ */
/* Bias pads go through the display utils (lcm_pinctl DT states vsp/vsn). */
static void m1621_bias_pad(unsigned long pin, int on)
{
	if (pin == M1621_BIAS_ENP_PIN && lcm_util.set_gpio_lcd_enp_bias)
		lcm_util.set_gpio_lcd_enp_bias(on);
	else if (pin == M1621_BIAS_ENN_PIN && lcm_util.set_gpio_lcd_enn_bias)
		lcm_util.set_gpio_lcd_enn_bias(on);
	else
		LCM_LOGI("bias pad %lu: no util function\n", pin);
}

/* ------------------------------------------------------------------ */
/* Init table (stock Flyme kernel, r63350_fhd_dsi_vdo_tcl_sharp)      */
/* ------------------------------------------------------------------ */
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{0xB0, 1, {0x04} },
	{0xB3, 6, {0x14, 0x00, 0x00, 0x00, 0x00, 0x00} },
	{0xB4, 2, {0x0C, 0x00} },
	{0xB6, 3, {0x4B, 0xDB, 0x16} },
	{0xB8, 7, {0x57, 0x3D, 0x19, 0x1E, 0x0A, 0x50, 0x50} },
	{0xB9, 7, {0x6F, 0x3D, 0x28, 0x3C, 0x14, 0xC8, 0xC8} },
	{0xBA, 7, {0xB5, 0x33, 0x41, 0x64, 0x23, 0xA0, 0xA0} },
	{0xBB, 2, {0x14, 0x14} },
	{0xBC, 2, {0x37, 0x32} },
	{0xBD, 2, {0x64, 0x32} },
	{0xC0, 1, {0xFF} },
	{0xC1, 34, {0x04, 0x60, 0x00, 0x40, 0x10, 0x00, 0x58, 0x03, 0x00, 0x00,
		    0x00, 0x64, 0x84, 0x30, 0x4A, 0x00, 0x00, 0x9D, 0xC1, 0x01,
		    0x00, 0xCA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00,
		    0x02, 0x20, 0x03, 0x98} },
	{0xC2, 8, {0x31, 0xF7, 0x80, 0x08, 0x08, 0x00, 0x00, 0x08} },
	{0xC3, 3, {0x00, 0x00, 0x00} },
	{0xC4, 11, {0x70, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x01,
		    0x06} },
	{0xC6, 21, {0x77, 0x01, 0x6E, 0x02, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x19, 0x0A,
		    0x77} },
	{0xC7, 30, {0x00, 0x11, 0x19, 0x23, 0x30, 0x3E, 0x47, 0x57, 0x3B, 0x42,
		    0x4D, 0x5A, 0x64, 0x6C, 0x77, 0x00, 0x11, 0x19, 0x23, 0x30,
		    0x3E, 0x47, 0x57, 0x3B, 0x42, 0x4D, 0x5A, 0x64, 0x6C, 0x77} },
	{0xC8, 55, {0x01, 0x00, 0x04, 0xFF, 0xFC, 0xE2, 0xFF, 0x00, 0x03, 0xFF,
		    0xFE, 0xFC, 0xF0, 0x00, 0x00, 0x04, 0x07, 0xF7, 0xE3, 0x00,
		    0x04, 0xFF, 0xFA, 0xFC, 0x00, 0x00, 0x03, 0xFF, 0xFD, 0xE6,
		    0x00, 0x00, 0x00, 0x03, 0x03, 0xBF, 0x00, 0x00, 0x04, 0xFF,
		    0xFB, 0xAD, 0x00, 0x00, 0x03, 0xFF, 0xFC, 0xDA, 0x00, 0x00,
		    0x00, 0x04, 0x09, 0xFC, 0x00} },
	{0xC9, 19, {0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0x00} },
	{0xCA, 43, {0x1D, 0xFC, 0xFC, 0xFC, 0x00, 0xCA, 0xCA, 0x96, 0x00, 0x96,
		    0xC0, 0xC0, 0x00, 0x82, 0x00, 0x00, 0x00, 0x9E, 0x00, 0x00,
		    0x00, 0x82, 0xFF, 0x9F, 0x83, 0xFF, 0x9F, 0x83, 0x7D, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00} },
	{0xCB, 15, {0xE6, 0xE0, 0xC7, 0x63, 0x00, 0x00, 0x00, 0x00, 0x20, 0xE0,
		    0x87, 0x00, 0xE8, 0x00, 0x00} },
	{0xCC, 1, {0x06} },
	{0xCE, 25, {0x55, 0x40, 0x49, 0x53, 0x59, 0x5E, 0x63, 0x68, 0x6E, 0x74,
		    0x7E, 0x8A, 0x98, 0xA8, 0xBB, 0xD0, 0xFF, 0x04, 0x00, 0x04,
		    0x04, 0x42, 0x00, 0x69, 0x5A} },
	{0xD0, 10, {0x55, 0x00, 0x00, 0x56, 0xCB, 0x40, 0x19, 0x19, 0x09, 0x00} },
	{0xD1, 4, {0x00, 0x48, 0x16, 0x0F} },
	{0xD3, 26, {0x1B, 0x33, 0x99, 0xBB, 0xB3, 0x33, 0x33, 0x33, 0x11, 0x00,
		    0x01, 0x00, 0x00, 0xE8, 0xA0, 0x02, 0x2F, 0x27, 0x33, 0x33,
		    0x72, 0x12, 0x8A, 0x57, 0x3D, 0xBC} },
	{0xD4, 4, {0x41, 0x04, 0x00, 0xD5} },
	{0xD6, 1, {0x01} },
	{0xD7, 24, {0xBF, 0xF8, 0x7F, 0xA8, 0xCE, 0x3E, 0xFC, 0xC1, 0xE1, 0xEF,
		    0x83, 0x07, 0x3F, 0x10, 0x7F, 0xC0, 0x01, 0xE7, 0x40, 0x3C,
		    0x00, 0xC0, 0x00, 0x00} },
	{0xD9, 3, {0x20, 0x00, 0x14} },
	{0xDD, 4, {0x30, 0x06, 0x23, 0x65} },
	{0xDE, 4, {0x00, 0x3F, 0xFF, 0x10} },
	{0x35, 1, {0x00} },
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 10, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} },
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		switch (table[i].cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;
		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V2(table[i].cmd, table[i].count,
					table[i].para_list, force_update);
		}
	}
}

/* ------------------------------------------------------------------ */
/* LCM driver                                                         */
/* ------------------------------------------------------------------ */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = 68;
	params->physical_height = 121;

	params->dsi.mode = SYNC_EVENT_VDO_MODE;
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.packet_size = 256;

	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 4;
	params->dsi.vertical_frontporch = 16;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 50;
	params->dsi.horizontal_frontporch = 90;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 460;
	params->dsi.ssc_disable = 1;
	params->dsi.CLK_HS_POST = 36;

	/* Stock enables ESD readback of reg 0x0A == 0x1C. Keep it off for the
	 * first bring-up so a DSI read problem cannot reset the panel loop. */
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;
}

static void lcm_init_power(void)
{
	SET_RESET_PIN(0);
	m1621_bias_pad(M1621_BIAS_ENP_PIN, 1);
	MDELAY(2);
	m1621_bias_pad(M1621_BIAS_ENN_PIN, 1);
	MDELAY(2);
	/* +5.5 V / -5.5 V */
	tps65132_write_bytes(0x00, 0x0F);
	tps65132_write_bytes(0x01, 0x0F);
	LCM_LOGI("init_power done\n");
}

static void lcm_suspend_power(void)
{
}

static void lcm_resume_power(void)
{
	lcm_init_power();
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(2);
	SET_RESET_PIN(1);
	MDELAY(6);

	push_table(lcm_initialization_setting,
		   ARRAY_SIZE(lcm_initialization_setting), 1);
	LCM_LOGI("init done\n");
}

static void lcm_suspend(void)
{
	unsigned int data_array[1];

	/* Plain sleep-in only. The stock driver also enters deep standby
	 * (B0 04, B1 01), which needs a working reset line to leave; keep that
	 * out until reset is proven on this kernel. */
	data_array[0] = 0x00280500;	/* display off */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);
	data_array[0] = 0x00100500;	/* sleep in */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(80);
	LCM_LOGI("suspend done\n");
}

static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_compare_id(void)
{
	return 1;
}

/* primary_display.c in this tree calls these Meizu diag hooks
 * unconditionally; the R63350 has none of those register pages. */
void lcm_m6_diag_read_stock_pages(void) { }
void lcm_m6_diag_page5_2a_probe(unsigned int value, unsigned int hold_ms) { }
void lcm_m6_diag_mode_ctrl_probe(unsigned int value, unsigned int hold_ms) { }

LCM_DRIVER r63350_fhd_dsi_vdo_tcl_sharp_lcm_drv = {
	.name = "r63350_fhd_dsi_vdo_tcl_sharp",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.suspend_power = lcm_suspend_power,
	.resume_power = lcm_resume_power,
};
