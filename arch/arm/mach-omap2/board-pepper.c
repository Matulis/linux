/*
 * Pepper
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/reboot.h>

#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#include <linux/phy.h>
#include <linux/micrel_phy.h>

#include <mach/hardware.h>

#include <asm/hardware/asp.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <plat/irqs.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/mmc.h>
#include <plat/usb.h>
#include <plat/lcdc.h>

#include <video/da8xx-fb.h>
#include <linux/input/ti_tscadc.h>

#include <linux/wl12xx.h>

#include "common.h"
#include "devices.h"
#include "hsmmc.h"
#include "mux.h"

/* Convert GPIO signal to GPIO pin number */
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))

static struct omap_board_config_kernel pepper_config[] __initdata = {
};

/* mmc */

static struct omap2_hsmmc_info pepper_mmc[] __initdata = {
	{
		.mmc            = 1,
		.caps           = MMC_CAP_4_BIT_DATA,
		.gpio_cd        = GPIO_TO_PIN(0, 6),
		.gpio_wp	= -EINVAL,
		.ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34, /* 3V3 */
	},
	{
		.mmc            = 2,
		.caps           = MMC_CAP_8_BIT_DATA,
		.nonremovable	= true,
		.gpio_cd        = -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34, /* 3V3 */
	},
	{
		.mmc            = 3,
		.caps           = MMC_CAP_4_BIT_DATA,
		.nonremovable	= true,
		.gpio_cd        = -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask       = 0x00100000, /* 3V3 */
	},
	{}      /* Terminator */
};

static struct regulator_consumer_supply pepper_vmmc3_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.2"),
};

static struct regulator_init_data pepper_vmmc3 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(pepper_vmmc3_supply),
	.consumer_supplies = pepper_vmmc3_supply,
};

static struct fixed_voltage_config pepper_wlan = {
	.supply_name		= "vwlan",
	.microvolts		= 3300000, /* 3.3V */
	.gpio			= -EINVAL,
	.startup_delay		= 0,
	.init_data		= &pepper_vmmc3,
};

static struct platform_device pepper_wlan_device = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data = &pepper_wlan,
	},
};

/* emif */

void __iomem *pepper_emif_base;

void __iomem * __init pepper_get_mem_ctlr(void)
{

	pepper_emif_base = ioremap(AM33XX_EMIF0_BASE, SZ_32K);

	if (!pepper_emif_base)
		pr_warning("%s: Unable to map DDR2 controller",	__func__);

	return pepper_emif_base;
}

void __iomem *am33xx_get_ram_base(void)
{
	return pepper_emif_base;
}

/* musb */
static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_ULPI,
	/*
	 * mode[0:3] = USB0PORT's mode
	 * mode[4:7] = USB1PORT's mode
	 * Pepper has USB0 in OTG mode and USB1 in host mode.
	 */
	.mode           = (MUSB_HOST << 4) | MUSB_OTG,
	.power		= 500,
	.instances	= 1,
};

/* i2c */

#include <linux/lis3lv02d.h>
#include <linux/mfd/tps65217.h>

static struct regulator_consumer_supply tps65217_dcdc1_consumers[] = {
	/* 1.8V */
	{
		.supply = "vdds_osc",
	},
	{
		.supply = "vdds_pll_ddr",
	},
	{
		.supply = "vdds_pll_mpu",
	},
	{
		.supply = "vdds_pll_core_lcd",
	},
	{
		.supply = "vdds_sram_mpu_bb",
	},
	{
		.supply = "vdds_sram_core_bg",
	},
	{
		.supply = "vdda_usb0_1p8v",
	},
	{
		.supply = "vdds_ddr",
	},
	{
		.supply = "vdds",
	},
	{
		.supply = "vdds_hvx_1p8v",
	},
	{
		.supply = "vdda_adc",
	},
	{
		.supply = "ddr2",
	},
};

static struct regulator_consumer_supply tps65217_dcdc2_consumers[] = {
	/* 1.1V */
	{
		.supply = "vdd_mpu",
	},
};

static struct regulator_consumer_supply tps65217_dcdc3_consumers[] = {
	/* 1.1V */
	{
		.supply = "vdd_core",
	},
};

static struct regulator_consumer_supply tps65217_ldo1_consumers[] = {
	/* 1.8V LDO */
	{
		.supply = "vdds_rtc",
	},
};

static struct regulator_consumer_supply tps65217_ldo2_consumers[] = {
	/* 3.3V LDO */
	{
		.supply = "vdds_any_pn",
	},
};

static struct regulator_consumer_supply tps65217_ldo3_consumers[] = {
	/* 3.3V LDO */
	{
		.supply = "vdds_hvx_ldo3_3p3v",
	},
	{
		.supply = "vdda_usb0_3p3v",
	},
};

static struct regulator_consumer_supply tps65217_ldo4_consumers[] = {
	/* 3.3V LDO */
	{
		.supply = "vdds_hvx_ldo4_3p3v",
	},
};

static struct regulator_init_data tps65217_regulator_data[] = {
	/* dcdc1 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 1800000,
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc1_consumers),
		.consumer_supplies = tps65217_dcdc1_consumers,
	},

	/* dcdc2 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc2_consumers),
		.consumer_supplies = tps65217_dcdc2_consumers,
	},

	/* dcdc3 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 1500000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc3_consumers),
		.consumer_supplies = tps65217_dcdc3_consumers,
	},

	/* ldo1 */
	{
		.constraints = {
			.min_uV = 1000000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo1_consumers),
		.consumer_supplies = tps65217_ldo1_consumers,
	},

	/* ldo2 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo2_consumers),
		.consumer_supplies = tps65217_ldo2_consumers,
	},

	/* ldo3 */
	{
		.constraints = {
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo3_consumers),
		.consumer_supplies = tps65217_ldo3_consumers,
	},

	/* ldo4 */
	{
		.constraints = {
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo4_consumers),
		.consumer_supplies = tps65217_ldo4_consumers,
	},
};

static struct tps65217_board pepper_tps65217_info = {
	.tps65217_init_data = &tps65217_regulator_data[0],
};

static struct lis3lv02d_platform_data lis331dlh_pdata = {
	.click_flags = LIS3_CLICK_SINGLE_X |
			LIS3_CLICK_SINGLE_Y |
			LIS3_CLICK_SINGLE_Z,
	.wakeup_flags = LIS3_WAKEUP_X_LO | LIS3_WAKEUP_X_HI |
			LIS3_WAKEUP_Y_LO | LIS3_WAKEUP_Y_HI |
			LIS3_WAKEUP_Z_LO | LIS3_WAKEUP_Z_HI,
	.irq_cfg = LIS3_IRQ1_CLICK | LIS3_IRQ2_CLICK,
	.wakeup_thresh	= 10,
	.click_thresh_x = 10,
	.click_thresh_y = 10,
	.click_thresh_z = 10,
	.g_range	= 2,
	.st_min_limits[0] = 120,
	.st_min_limits[1] = 120,
	.st_min_limits[2] = 140,
	.st_max_limits[0] = 550,
	.st_max_limits[1] = 550,
	.st_max_limits[2] = 750,
};

static struct i2c_board_info pepper_i2c_boardinfo1[] = {
	{
		I2C_BOARD_INFO("tps65217", TPS65217_I2C_ID),
		.platform_data  = &pepper_tps65217_info,
	},
	{
		I2C_BOARD_INFO("tlv320aic3x", 0x1b),
	},
	{
		I2C_BOARD_INFO("lis331dlh", 0x1d),
		.platform_data = &lis331dlh_pdata,
	},
};

static struct i2c_board_info pepper_i2c_boardinfo2[] = {
};

static void pepper_i2c_init(void)
{
	omap_register_i2c_bus(1, 100, pepper_i2c_boardinfo1,
			ARRAY_SIZE(pepper_i2c_boardinfo1));
	omap_register_i2c_bus(2, 100, pepper_i2c_boardinfo2,
			ARRAY_SIZE(pepper_i2c_boardinfo2));
	return;
}

/* sound */

static u8 am335x_iis_serializer_direction1[] = {
	TX_MODE,	RX_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
};

static struct snd_platform_data pepper_snd_data1 = {
	.tx_dma_offset	= 0x46000000,	/* McASP0 */
	.rx_dma_offset	= 0x46000000,
	.op_mode	= DAVINCI_MCASP_IIS_MODE,
	.num_serializer	= ARRAY_SIZE(am335x_iis_serializer_direction1),
	.tdm_slots	= 2,
	.serial_dir	= am335x_iis_serializer_direction1,
	.asp_chan_q	= EVENTQ_2,
	.version	= MCASP_VERSION_3,
	.txnumevt	= 1,
	.rxnumevt	= 1,
};

/* lcd */
#define GPIO_LCD_ENABLE	GPIO_TO_PIN(1, 27)

static const struct display_panel disp_panel = {
	WVGA,
	32,
	32,
	COLOR_ACTIVE,
};

static struct lcd_ctrl_config lcd_cfg = {
	&disp_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 32,
	.fdd			= 0x80,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

static void pepper_panel_power_ctrl(int val)
{
	gpio_set_value(GPIO_LCD_ENABLE, val);
}

struct da8xx_lcdc_platform_data lcdc_pdata = {
	.manu_name		= "Samsung",
	.controller_data	= &lcd_cfg,
	.type			= "Samsung_LMS430",
	.panel_power_ctrl	= pepper_panel_power_ctrl,
};

static int __init conf_disp_pll(int rate)
{
	struct clk *disp_pll;
	int ret = -EINVAL;

	disp_pll = clk_get(NULL, "dpll_disp_ck");
	if (IS_ERR(disp_pll)) {
		pr_err("Cannot clk_get disp_pll\n");
		goto out;
	}

	ret = clk_set_rate(disp_pll, rate);
	clk_put(disp_pll);
out:
	return ret;
}

/* touchscreen controller */
static struct tsc_data am335x_touchscreen_data  = {
	.wires  = 4,
	.x_plate_resistance = 200,
};

/* Enable clkout2 */
struct pinmux_config {
	const char *string_name; /* signal name format */
	int val; /* Options for the mux register value */
};

static void setup_pin_mux(struct pinmux_config *pin_mux)
{
	int i;

	for (i = 0; pin_mux->string_name != NULL; pin_mux++)
		omap_mux_init_signal(pin_mux->string_name, pin_mux->val);

}

static struct pinmux_config clkout2_pin_mux[] = {
	{"xdma_event_intr1.clkout2", OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static void __init clkout2_enable(void)
{
	struct clk *ck_32;

	ck_32 = clk_get(NULL, "clkout2_ck");
	if (IS_ERR(ck_32)) {
		pr_err("Cannot clk_get ck_32\n");
		return;
	}

	clk_enable(ck_32);
	setup_pin_mux(clkout2_pin_mux);
}

/* rtc */
static struct resource am335x_rtc_resources[] = {
	{
		.start		= AM33XX_RTC_BASE,
		.end		= AM33XX_RTC_BASE + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
	{ /* timer irq */
		.start		= AM33XX_IRQ_RTC_TIMER,
		.end		= AM33XX_IRQ_RTC_TIMER,
		.flags		= IORESOURCE_IRQ,
	},
	{ /* alarm irq */
		.start		= AM33XX_IRQ_RTC_ALARM,
		.end		= AM33XX_IRQ_RTC_ALARM,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device am335x_rtc_device = {
	.name           = "omap_rtc",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(am335x_rtc_resources),
	.resource	= am335x_rtc_resources,
};

static int am335x_rtc_init(void)
{
	void __iomem *base;
	struct clk *clk;

	clk = clk_get(NULL, "rtc_fck");
	if (IS_ERR(clk)) {
		pr_err("rtc : Failed to get RTC clock\n");
		return -1;
	}

	if (clk_enable(clk)) {
		pr_err("rtc: Clock Enable Failed\n");
		return -1;
	}

	base = ioremap(AM33XX_RTC_BASE, SZ_4K);

	if (WARN_ON(!base))
		return -ENOMEM;

	/* Unlock the rtc's registers */
	writel(0x83e70b13, base + 0x6c);
	writel(0x95a4f1e0, base + 0x70);

	/*
	 * Enable the 32K OSc
	 * TODO: Need a better way to handle this
	 * Since we want the clock to be running before mmc init
	 * we need to do it before the rtc probe happens
	 */
	writel(0x48, base + 0x54);

	iounmap(base);

	return  platform_device_register(&am335x_rtc_device);
}

/* board init */

static int ksz9021rn_phy_fixup(struct phy_device *phydev)
{
	/* max rx/tx clock delay, min rx/tx control delay */
	phy_write(phydev, 0x0b, 0x8104);
	phy_write(phydev, 0x0c, 0xa0a0);

	/* min rx data delay */
	phy_write(phydev, 0x0b, 0x8105);
	phy_write(phydev, 0x0c, 0x0000);

	/* min tx data delay */
	phy_write(phydev, 0x0b, 0x8106);
	phy_write(phydev, 0x0c, 0x0000);

	return 0;
}

static void __init pepper_init(void)
{
	omap_serial_init();
	am335x_rtc_init();
	clkout2_enable();
	pepper_i2c_init();
	omap2_hsmmc_init(pepper_mmc);
	platform_device_register(&pepper_wlan_device);
	pepper_get_mem_ctlr();
	omap_sdrc_init(NULL, NULL);
	usb_musb_init(&musb_board_data);
	if (IS_ENABLED(CONFIG_PHYLIB))
		phy_register_fixup_for_uid(PHY_ID_KSZ9021, MICREL_PHY_ID_MASK,
				ksz9021rn_phy_fixup);
	am33xx_cpsw_init(AM33XX_CPSW_MODE_RGMII, NULL, NULL);

	/* audio init */
	gpio_request(48, "audio nreset");
	gpio_export(48, 0);
	gpio_direction_output(48, 0);
	gpio_set_value(48, 0);
	gpio_set_value(48, 1);
	am335x_register_mcasp(&pepper_snd_data1, 0);

	/* lcd init */
	gpio_request(59, "lcd enable");
	gpio_export(59, 0);
	gpio_direction_output(59, 0);
	gpio_set_value(59, 1);
	conf_disp_pll(18400000);
	if (am33xx_register_lcdc(&lcdc_pdata))
		pr_info("Failed to register LCDC device\n");

	/* touchscreen init */
	if (am33xx_register_tsc(&am335x_touchscreen_data))
		pr_err("failed to register touchscreen device\n");

	omap_board_config = pepper_config;
	omap_board_config_size = ARRAY_SIZE(pepper_config);
}

static void __init pepper_map_io(void)
{
	omap2_set_globals_am33xx();
	omapam33xx_map_common_io();
}

MACHINE_START(PEPPER, "pepper")
	.atag_offset	= 0x100,
	.map_io		= pepper_map_io,
	.init_early	= am33xx_init_early,
	.init_irq	= ti81xx_init_irq,
	.handle_irq     = omap3_intc_handle_irq,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= pepper_init,
MACHINE_END

