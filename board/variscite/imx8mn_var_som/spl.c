/*
 * Copyright 2018 NXP
 * Copyright 2019 Variscite Ltd.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/imx8mn_pins.h>
#include <asm/arch/sys_proto.h>
#include <power/pmic.h>
#include <power/bd71837.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8m_ddr.h>

#include "../common/imx8_eeprom.h"

DECLARE_GLOBAL_DATA_PTR;

enum {
	SOM_REV10,
	SOM_REV11,
	UNKNOWN_REV,
};

static int get_som_rev(void)
{
	struct var_eeprom eeprom = {0};
	static int som_rev = UNKNOWN_REV;

	if (som_rev != UNKNOWN_REV)
		return som_rev;

	var_eeprom_read_header(&eeprom);

	if (!var_eeprom_is_valid(&eeprom)) {
		printf("No SOM rev info in EEPROM, assuming REV1.1+\n");
		som_rev = SOM_REV11;
	}
	else if (eeprom.somrev == 0)
		som_rev = SOM_REV10;
	else
		som_rev = SOM_REV11;

	return som_rev;
}

void spl_dram_init(void)
{
	struct var_eeprom eeprom;

	var_eeprom_read_header(&eeprom);
	var_eeprom_adjust_dram(&eeprom, &dram_timing);
	ddr_init(&dram_timing);
}

#define I2C_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PE)
#define PC 		MUX_PAD_CTRL(I2C_PAD_CTRL)

struct i2c_pads_info i2c_pad_info1 = {
	.scl = {
		.i2c_mode = IMX8MN_PAD_I2C1_SCL__I2C1_SCL | PC,
		.gpio_mode = IMX8MN_PAD_I2C1_SCL__GPIO5_IO14 | PC,
		.gp = IMX_GPIO_NR(5, 14),
	},
	.sda = {
		.i2c_mode = IMX8MN_PAD_I2C1_SDA__I2C1_SDA | PC,
		.gpio_mode = IMX8MN_PAD_I2C1_SDA__GPIO5_IO15 | PC,
		.gp = IMX_GPIO_NR(5, 15),
	},
};

#define USDHC2_CD_GPIO_SOM_REV10	IMX_GPIO_NR(2, 12)
#define USDHC2_CD_GPIO			IMX_GPIO_NR(1, 10)
#define USDHC2_PWR_GPIO 		IMX_GPIO_NR(4, 22)
#define USDHC3_PWR_GPIO 		IMX_GPIO_NR(3, 16)

#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE |PAD_CTL_PE | \
			 PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)
#define USDHC_CD_PAD_CTRL (PAD_CTL_PE |PAD_CTL_PUE |PAD_CTL_HYS | PAD_CTL_DSE4)

static iomux_v3_cfg_t const usdhc3_pads[] = {
	IMX8MN_PAD_NAND_WE_B__USDHC3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_WP_B__USDHC3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA04__USDHC3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA05__USDHC3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA06__USDHC3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA07__USDHC3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_RE_B__USDHC3_DATA4   | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_CE2_B__USDHC3_DATA5  | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_CE3_B__USDHC3_DATA6  | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_CLE__USDHC3_DATA7    | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_NAND_READY_B__GPIO3_IO16  | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_pads[] = {
	IMX8MN_PAD_SD2_CLK__USDHC2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_CMD__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_DATA0__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_DATA1__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_DATA2__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_DATA3__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_GPIO1_IO04__USDHC2_VSELECT | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SAI2_RXC__GPIO4_IO22 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_cd_pads_som_rev10[] = {
	IMX8MN_PAD_SD2_CD_B__GPIO2_IO12 | MUX_PAD_CTRL(USDHC_CD_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_cd_pads_som_rev11[] = {
	IMX8MN_PAD_GPIO1_IO10__GPIO1_IO10 | MUX_PAD_CTRL(USDHC_CD_PAD_CTRL),
};

static struct fsl_esdhc_cfg usdhc_cfg[2] = {
	{USDHC2_BASE_ADDR, 0, 4},
	{USDHC3_BASE_ADDR, 0, 8},
};

int board_mmc_init(bd_t *bis)
{
	int i, ret;
	iomux_v3_cfg_t const *usdhc2_cd_pads;
	int usdhc2_cd_gpio;

	if (get_som_rev() == SOM_REV10) {
		usdhc2_cd_pads = usdhc2_cd_pads_som_rev10;
		usdhc2_cd_gpio = USDHC2_CD_GPIO_SOM_REV10;
	}
	else {
		usdhc2_cd_pads = usdhc2_cd_pads_som_rev11;
		usdhc2_cd_gpio = USDHC2_CD_GPIO;
	}

	/*
	 * According to the board_mmc_init() the following map is done:
	 * (U-Boot device node)    (Physical Port)
	 * mmc0                    USDHC2
	 * mmc1                    USDHC3
	 */
	for (i = 0; i < CONFIG_SYS_FSL_USDHC_NUM; i++) {
		switch (i) {
		case 0:
			usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
			imx_iomux_v3_setup_multiple_pads(
				usdhc2_pads, ARRAY_SIZE(usdhc2_pads));
			gpio_request(USDHC2_PWR_GPIO, "usdhc2_reset");
			gpio_direction_output(USDHC2_PWR_GPIO, 0);
			mdelay(10);
			gpio_direction_output(USDHC2_PWR_GPIO, 1);

			imx_iomux_v3_setup_multiple_pads(
				usdhc2_cd_pads, ARRAY_SIZE(usdhc2_cd_pads));
			gpio_request(usdhc2_cd_gpio, "usdhc2_cd");
			gpio_direction_input(usdhc2_cd_gpio);
			break;
		case 1:
			usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
			imx_iomux_v3_setup_multiple_pads(
				usdhc3_pads, ARRAY_SIZE(usdhc3_pads));
			gpio_request(USDHC3_PWR_GPIO, "usdhc3_reset");
			gpio_direction_output(USDHC3_PWR_GPIO, 0);
			mdelay(10);
			gpio_direction_output(USDHC3_PWR_GPIO, 1);
			break;
		default:
			printf("Warning: you configured more USDHC controllers"
				"(%d) than supported by the board\n", i + 1);
			return -EINVAL;
		}

		ret = fsl_esdhc_initialize(bis, &usdhc_cfg[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;
	int usdhc2_cd_gpio;

	if (get_som_rev() == SOM_REV10)
		usdhc2_cd_gpio = USDHC2_CD_GPIO_SOM_REV10;
	else
		usdhc2_cd_gpio = USDHC2_CD_GPIO;

	switch (cfg->esdhc_base) {
	case USDHC3_BASE_ADDR:
		ret = 1;
		break;
	case USDHC2_BASE_ADDR:
		ret = !gpio_get_value(usdhc2_cd_gpio);
		return ret;
	}

	return 1;
}

#ifdef CONFIG_POWER
#define I2C_PMIC	0
int power_init_board(void)
{
	struct pmic *p;
	int ret;

	ret = power_bd71837_init(I2C_PMIC);
	if (ret)
		printf("power init failed");

	p = pmic_get("BD71837");
	pmic_probe(p);

	/* decrease RESET key long push time from the default 10s to 10ms */
	pmic_reg_write(p, BD71837_PWRONCONFIG1, 0x0);

	/* unlock the PMIC regs */
	pmic_reg_write(p, BD71837_REGLOCK, 0x1);

#ifdef CONFIG_IMX8MN_FORCE_NOM_SOC
	/* increase VDD_ARM to typical value 0.85v for 1.2Ghz */
	pmic_reg_write(p, BD71837_BUCK2_VOLT_RUN, 0xf);

	/* increase VDD_SOC/VDD_DRAM to typical value 0.85v for nominal mode */
	pmic_reg_write(p, BD71837_BUCK1_VOLT_RUN, 0xf);
#else
	/* increase VDD_SOC/VDD_DRAM to typical value 0.95v for 3Ghz DDRs */
	pmic_reg_write(p, BD71837_BUCK1_VOLT_RUN, 0x19);
#endif

	/* Set VDD_SOC 0.85v for suspend */
	pmic_reg_write(p, BD71837_BUCK1_VOLT_SUSP, 0xf);

#ifdef CONFIG_IMX8M_DDR4
	/* increase NVCC_DRAM_1V2 to 1.2v for DDR4 */
	pmic_reg_write(p, BD71837_BUCK8_VOLT, 0x28);
#endif
	/* lock the PMIC regs */
	pmic_reg_write(p, BD71837_REGLOCK, 0x11);

	return 0;
}
#endif

void spl_board_init(void)
{
#ifndef CONFIG_SPL_USB_SDP_SUPPORT
	/* Serial download mode */
	if (is_usb_boot()) {
		puts("Back to ROM, SDP\n");
		restore_boot_params();
	}
#endif
	puts("Normal Boot\n");
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	int rev = get_som_rev();

	if ((rev == SOM_REV11) && !strcmp(name, "fsl-imx8mn-var-som"))
		return 0;
	else if ((rev == SOM_REV10) && !strcmp(name, "fsl-imx8mn-var-som-rev10"))
		return 0;
	else
		return -1;
}
#endif

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)

static iomux_v3_cfg_t const uart4_pads[] = {
	IMX8MN_PAD_UART4_RXD__UART4_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MN_PAD_UART4_TXD__UART4_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

int board_early_init_f(void)
{
	imx_iomux_v3_setup_multiple_pads(uart4_pads, ARRAY_SIZE(uart4_pads));

	return 0;
}

void board_init_f(ulong dummy)
{
	int ret;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	board_early_init_f();

	timer_init();

	preloader_console_init();

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	enable_tzc380();

	/* Adjust pmic voltage to 1.0V for 800M */
	setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info1);

	power_init_board();

	/* DDR initialization */
	spl_dram_init();

	board_init_r(NULL, 0);
}

#ifdef CONFIG_SPL_MMC_SUPPORT

#define UBOOT_RAW_SECTOR_OFFSET 0x40
unsigned long spl_mmc_get_uboot_raw_sector(struct mmc *mmc)
{
	u32 boot_dev = spl_boot_device();
	switch (boot_dev) {
		case BOOT_DEVICE_MMC1:
			return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR;
		case BOOT_DEVICE_MMC2:
			return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR - UBOOT_RAW_SECTOR_OFFSET;
	}
	return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR;
}
#endif