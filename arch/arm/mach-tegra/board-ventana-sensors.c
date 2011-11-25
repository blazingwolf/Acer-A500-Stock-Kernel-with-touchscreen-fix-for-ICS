/*
 * arch/arm/mach-tegra/board-ventana-sensors.c
 *
 * Copyright (c) 2011, NVIDIA CORPORATION, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of NVIDIA CORPORATION nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/akm8975.h>
#include <linux/mpu.h>
#include <linux/i2c/pca954x.h>
#include <linux/i2c/pca953x.h>
#include <linux/nct1008.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include <mach/gpio.h>

#include <media/ov5650.h>
#include <media/ov2710.h>
#include <media/ssl3250a.h>
#include <media/yuv_sensor.h>
#include <media/yuv5_sensor.h>

#include <generated/mach-types.h>

#include "gpio-names.h"
#include "board.h"
#include "board-ventana.h"

#define AL3000A_IRQ_GPIO	TEGRA_GPIO_PZ2
#define AKM8975_IRQ_GPIO	TEGRA_GPIO_PN5
#define CAMERA_POWER_GPIO	TEGRA_GPIO_PV4
#define CAMERA_CSI_MUX_SEL_GPIO	TEGRA_GPIO_PBB4
#define CAMERA_FLASH_ACT_GPIO	TEGRA_GPIO_PD2
#define CAMERA_FLASH_STRB_GPIO	TEGRA_GPIO_PA0
#define AC_PRESENT_GPIO		TEGRA_GPIO_PV3
#define NCT1008_THERM2_GPIO	TEGRA_GPIO_PN6

#ifdef CONFIG_VIDEO_OV5650
#define OV5650_PWDN_GPIO      TEGRA_GPIO_PL0
#define OV5650_RST_GPIO       TEGRA_GPIO_PL6
#endif
#ifdef CONFIG_VIDEO_YUV
#define YUV_SENSOR_OE_GPIO    TEGRA_GPIO_PL2
#define YUV_SENSOR_RST_GPIO   TEGRA_GPIO_PL4
#endif
#ifdef CONFIG_VIDEO_YUV5
#define YUV5_SENSOR_PWDN_GPIO TEGRA_GPIO_PL0
#define YUV5_SENSOR_RST_GPIO  TEGRA_GPIO_PL6
#endif

#define CAMERA_FLASH_OP_MODE		0 /*0=I2C mode, 1=GPIO mode*/
#define CAMERA_FLASH_MAX_LED_AMP	7
#define CAMERA_FLASH_MAX_TORCH_AMP	11
#define CAMERA_FLASH_MAX_FLASH_AMP	31

#if defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)  \
	|| defined(CONFIG_MACH_ACER_VANGOGH)
struct camera_gpios {
	const char *name;
	int gpio;
};

#define CAMERA_GPIO(_name, _gpio)  \
	{                          \
		.name = _name,     \
		.gpio = _gpio,     \
	}
#endif

#ifdef CONFIG_VIDEO_OV5650
static struct camera_gpios ov5650_gpio_keys[] = {
	[0] = CAMERA_GPIO("cam_power_en", CAMERA_POWER_GPIO),
	[1] = CAMERA_GPIO("ov5650_pwdn", OV5650_PWDN_GPIO),
	[2] = CAMERA_GPIO("ov5650_rst", OV5650_RST_GPIO),
};

static int ov5650_power_on(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(ov5650_gpio_keys); i++) {
		tegra_gpio_enable(ov5650_gpio_keys[i].gpio);
		ret = gpio_request(ov5650_gpio_keys[i].gpio,
				ov5650_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail;
		}
	}

	gpio_direction_output(OV5650_PWDN_GPIO, 1);
	gpio_direction_output(OV5650_RST_GPIO, 1);
	msleep(1);
	gpio_direction_output(CAMERA_POWER_GPIO, 1);
	msleep(5);
	gpio_direction_output(OV5650_PWDN_GPIO, 0);
	msleep(20);
	gpio_direction_output(OV5650_RST_GPIO, 0);
	msleep(1);
	gpio_direction_output(OV5650_RST_GPIO, 1);
	msleep(20);

	return 0;

fail:
	while (i--)
		gpio_free(ov5650_gpio_keys[i].gpio);
	return ret;
}

static int ov5650_power_off(void)
{
	int i;

	gpio_direction_output(OV5650_PWDN_GPIO, 1);
	gpio_direction_output(CAMERA_POWER_GPIO, 0);
	gpio_direction_output(OV5650_PWDN_GPIO, 0);
	gpio_direction_output(OV5650_RST_GPIO, 0);

	i = ARRAY_SIZE(ov5650_gpio_keys);
	while (i--)
		gpio_free(ov5650_gpio_keys[i].gpio);

	return 0;
}

struct ov5650_platform_data ov5650_data = {
	.power_on = ov5650_power_on,
	.power_off = ov5650_power_off,
};
#endif /* CONFIG_VIDEO_OV5650 */

#ifdef CONFIG_VIDEO_YUV
static struct camera_gpios yuv_sensor_gpio_keys[] = {
	[0] = CAMERA_GPIO("cam_power_en", CAMERA_POWER_GPIO),
	[1] = CAMERA_GPIO("yuv_sensor_oe", YUV_SENSOR_OE_GPIO),
	[2] = CAMERA_GPIO("yuv_sensor_rst", YUV_SENSOR_RST_GPIO),
};

static int yuv_sensor_power_on(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(yuv_sensor_gpio_keys); i++) {
		tegra_gpio_enable(yuv_sensor_gpio_keys[i].gpio);
		ret = gpio_request(yuv_sensor_gpio_keys[i].gpio,
				yuv_sensor_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail;
		}
	}


	gpio_direction_output(YUV_SENSOR_OE_GPIO, 0);
	gpio_direction_output(YUV_SENSOR_RST_GPIO, 1);
	msleep(1);
	gpio_direction_output(CAMERA_POWER_GPIO, 1);
	msleep(1);
	gpio_direction_output(YUV_SENSOR_RST_GPIO, 0);
	msleep(1);
	gpio_direction_output(YUV_SENSOR_RST_GPIO, 1);

	return 0;

fail:
	while (i--)
		gpio_free(yuv_sensor_gpio_keys[i].gpio);
	return ret;
}

static int yuv_sensor_power_off(void)
{
	int i;

	gpio_direction_output(YUV_SENSOR_OE_GPIO, 1);
	gpio_direction_output(CAMERA_POWER_GPIO, 0);
	gpio_direction_output(YUV_SENSOR_OE_GPIO, 0);
	gpio_direction_output(YUV_SENSOR_RST_GPIO, 0);

	i = ARRAY_SIZE(yuv_sensor_gpio_keys);
	while (i--)
		gpio_free(yuv_sensor_gpio_keys[i].gpio);

	return 0;
}

struct yuv_sensor_platform_data yuv_sensor_data = {
	.power_on = yuv_sensor_power_on,
	.power_off = yuv_sensor_power_off,
};
#endif /* CONFIG_VIDEO_YUV */

#ifdef CONFIG_VIDEO_YUV5
static struct camera_gpios yuv5_sensor_gpio_keys[] = {
	[0] = CAMERA_GPIO("cam_power_en", CAMERA_POWER_GPIO),
	[1] = CAMERA_GPIO("yuv5_sensor_pwdn", YUV5_SENSOR_PWDN_GPIO),
	[2] = CAMERA_GPIO("yuv5_sensor_rst", YUV5_SENSOR_RST_GPIO),
};

static int yuv5_sensor_power_on(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(yuv5_sensor_gpio_keys); i++) {
		tegra_gpio_enable(yuv5_sensor_gpio_keys[i].gpio);
		ret = gpio_request(yuv5_sensor_gpio_keys[i].gpio,
				yuv5_sensor_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail;
		}
	}

	gpio_direction_output(YUV5_SENSOR_PWDN_GPIO, 0);
	gpio_direction_output(YUV5_SENSOR_RST_GPIO, 0);
	msleep(1);
	gpio_direction_output(CAMERA_POWER_GPIO, 1);
	msleep(1);
	gpio_direction_output(YUV5_SENSOR_RST_GPIO, 1);
	msleep(1);

	return 0;

fail:
	while (i--)
	gpio_free(yuv5_sensor_gpio_keys[i].gpio);
	return ret;
}

static int yuv5_sensor_power_off(void)
{
	int i;

	gpio_direction_output(YUV5_SENSOR_RST_GPIO, 0);
	msleep(1);
	gpio_direction_output(YUV5_SENSOR_PWDN_GPIO, 1);
	gpio_direction_output(CAMERA_POWER_GPIO, 0);
	gpio_direction_output(YUV5_SENSOR_PWDN_GPIO, 0);

	i = ARRAY_SIZE(yuv5_sensor_gpio_keys);
	while (i--)
		gpio_free(yuv5_sensor_gpio_keys[i].gpio);

	return 0;
};

struct yuv5_sensor_platform_data yuv5_sensor_data = {
	.power_on = yuv5_sensor_power_on,
	.power_off = yuv5_sensor_power_off,
};
#endif /* CONFIG_VIDEO_YUV5 */

#if defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)  \
	|| defined(CONFIG_MACH_ACER_VANGOGH)
static int ventana_camera_init(void)
{
	int i, ret;

#ifdef CONFIG_VIDEO_OV5650
	// initialize OV5650
	for (i = 0; i < ARRAY_SIZE(ov5650_gpio_keys); i++) {
		tegra_gpio_enable(ov5650_gpio_keys[i].gpio);
		ret = gpio_request(ov5650_gpio_keys[i].gpio,
				ov5650_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail1;
		}
	}

	gpio_direction_output(CAMERA_POWER_GPIO, 0);
	gpio_direction_output(OV5650_PWDN_GPIO, 0);
	gpio_direction_output(OV5650_RST_GPIO, 0);

	for (i = 0; i < ARRAY_SIZE(ov5650_gpio_keys); i++) {
		gpio_free(ov5650_gpio_keys[i].gpio);
	}
#endif

#ifdef CONFIG_VIDEO_YUV
	// initialize MT9D115
	for (i = 0; i < ARRAY_SIZE(yuv_sensor_gpio_keys); i++) {
		tegra_gpio_enable(yuv_sensor_gpio_keys[i].gpio);
		ret = gpio_request(yuv_sensor_gpio_keys[i].gpio,
				yuv_sensor_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail2;
		}
	}

	gpio_direction_output(CAMERA_POWER_GPIO, 0);
	gpio_direction_output(YUV_SENSOR_OE_GPIO, 0);
	gpio_direction_output(YUV_SENSOR_RST_GPIO, 0);

	for (i = 0; i < ARRAY_SIZE(yuv_sensor_gpio_keys); i++) {
		gpio_free(yuv_sensor_gpio_keys[i].gpio);
	}
#endif

#ifdef CONFIG_VIDEO_YUV5
	// initialize MT9P111
	for (i = 0; i < ARRAY_SIZE(yuv5_sensor_gpio_keys); i++) {
		tegra_gpio_enable(yuv5_sensor_gpio_keys[i].gpio);
		ret = gpio_request(yuv5_sensor_gpio_keys[i].gpio,
				yuv5_sensor_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail3;
		}
	}

	gpio_direction_output(CAMERA_POWER_GPIO, 0);
	gpio_direction_output(YUV5_SENSOR_PWDN_GPIO, 0);
	gpio_direction_output(YUV5_SENSOR_RST_GPIO, 0);

	for (i = 0; i < ARRAY_SIZE(yuv5_sensor_gpio_keys); i++) {
		gpio_free(yuv5_sensor_gpio_keys[i].gpio);
	}
#endif
	return 0;

#ifdef CONFIG_VIDEO_OV5650
fail1:
	while (i--)
		gpio_free(ov5650_gpio_keys[i].gpio);
	return ret;
#endif

#ifdef CONFIG_VIDEO_YUV
fail2:
        while (i--)
                gpio_free(yuv_sensor_gpio_keys[i].gpio);
	return ret;
#endif

#ifdef CONFIG_VIDEO_YUV5
fail3:
	while (i--)
		gpio_free(yuv5_sensor_gpio_keys[i].gpio);
	return ret;
#endif
}
#endif /* PICASSO || MAYA || VANGOGH */

extern void tegra_throttling_enable(bool enable);

#if !(defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)  \
	|| defined(CONFIG_MACH_ACER_VANGOGH))
static int ventana_camera_init(void)
{
	tegra_gpio_enable(CAMERA_POWER_GPIO);
	gpio_request(CAMERA_POWER_GPIO, "camera_power_en");
	gpio_direction_output(CAMERA_POWER_GPIO, 1);
	gpio_export(CAMERA_POWER_GPIO, false);

	tegra_gpio_enable(CAMERA_CSI_MUX_SEL_GPIO);
	gpio_request(CAMERA_CSI_MUX_SEL_GPIO, "camera_csi_sel");
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 0);
	gpio_export(CAMERA_CSI_MUX_SEL_GPIO, false);

	return 0;
}

/* left ov5650 is CAM2 which is on csi_a */
static int ventana_left_ov5650_power_on(void)
{
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 0);
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 1);
	gpio_direction_output(CAM2_LDO_SHUTDN_L_GPIO, 1);
	mdelay(5);
	gpio_direction_output(CAM2_PWR_DN_GPIO, 0);
	mdelay(5);
	gpio_direction_output(CAM2_RST_L_GPIO, 0);
	mdelay(1);
	gpio_direction_output(CAM2_RST_L_GPIO, 1);
	mdelay(20);
	return 0;
}

static int ventana_left_ov5650_power_off(void)
{
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 0);
	gpio_direction_output(CAM2_RST_L_GPIO, 0);
	gpio_direction_output(CAM2_PWR_DN_GPIO, 1);
	gpio_direction_output(CAM2_LDO_SHUTDN_L_GPIO, 0);
	return 0;
}

struct ov5650_platform_data ventana_left_ov5650_data = {
	.power_on = ventana_left_ov5650_power_on,
	.power_off = ventana_left_ov5650_power_off,
};

/* right ov5650 is CAM1 which is on csi_b */
static int ventana_right_ov5650_power_on(void)
{
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 1);
	gpio_direction_output(CAM1_LDO_SHUTDN_L_GPIO, 1);
	mdelay(5);
	gpio_direction_output(CAM1_PWR_DN_GPIO, 0);
	mdelay(5);
	gpio_direction_output(CAM1_RST_L_GPIO, 0);
	mdelay(1);
	gpio_direction_output(CAM1_RST_L_GPIO, 1);
	mdelay(20);
	return 0;
}

static int ventana_right_ov5650_power_off(void)
{
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 0);
	gpio_direction_output(CAM1_RST_L_GPIO, 0);
	gpio_direction_output(CAM1_PWR_DN_GPIO, 1);
	gpio_direction_output(CAM1_LDO_SHUTDN_L_GPIO, 0);
	return 0;
}

struct ov5650_platform_data ventana_right_ov5650_data = {
	.power_on = ventana_right_ov5650_power_on,
	.power_off = ventana_right_ov5650_power_off,
};
#endif /* !(PICASSO || MAYA || VANGOGH) */

#ifdef CONFIG_VIDEO_OV2710
static int ventana_ov2710_power_on(void)
{
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 1);
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 1);
	gpio_direction_output(CAM3_LDO_SHUTDN_L_GPIO, 1);
	mdelay(5);
	gpio_direction_output(CAM3_PWR_DN_GPIO, 0);
	mdelay(5);
	gpio_direction_output(CAM3_RST_L_GPIO, 0);
	mdelay(1);
	gpio_direction_output(CAM3_RST_L_GPIO, 1);
	mdelay(20);
	return 0;
}

static int ventana_ov2710_power_off(void)
{
	gpio_direction_output(CAM3_RST_L_GPIO, 0);
	gpio_direction_output(CAM3_PWR_DN_GPIO, 1);
	gpio_direction_output(CAM3_LDO_SHUTDN_L_GPIO, 0);
	gpio_direction_output(AVDD_DSI_CSI_ENB_GPIO, 0);
	gpio_direction_output(CAMERA_CSI_MUX_SEL_GPIO, 0);
	return 0;
}

struct ov2710_platform_data ventana_ov2710_data = {
	.power_on = ventana_ov2710_power_on,
	.power_off = ventana_ov2710_power_off,
};
#endif /* CONFIG_VIDEO_OV2710 */

#ifdef CONFIG_TORCH_SSL3250A
static int ventana_ssl3250a_init(void)
{
	gpio_request(CAMERA_FLASH_ACT_GPIO, "torch_gpio_act");
	gpio_direction_output(CAMERA_FLASH_ACT_GPIO, 0);
	tegra_gpio_enable(CAMERA_FLASH_ACT_GPIO);
	gpio_request(CAMERA_FLASH_STRB_GPIO, "torch_gpio_strb");
	gpio_direction_output(CAMERA_FLASH_STRB_GPIO, 0);
	tegra_gpio_enable(CAMERA_FLASH_STRB_GPIO);
	gpio_export(CAMERA_FLASH_STRB_GPIO, false);
	return 0;
}

static void ventana_ssl3250a_exit(void)
{
	gpio_set_value(CAMERA_FLASH_STRB_GPIO, 0);
	gpio_free(CAMERA_FLASH_STRB_GPIO);
	tegra_gpio_disable(CAMERA_FLASH_STRB_GPIO);
	gpio_set_value(CAMERA_FLASH_ACT_GPIO, 0);
	gpio_free(CAMERA_FLASH_ACT_GPIO);
	tegra_gpio_disable(CAMERA_FLASH_ACT_GPIO);
}

static int ventana_ssl3250a_gpio_strb(int val)
{
	int prev_val;
	prev_val = gpio_get_value(CAMERA_FLASH_STRB_GPIO);
	gpio_set_value(CAMERA_FLASH_STRB_GPIO, val);
	return prev_val;
};

static int ventana_ssl3250a_gpio_act(int val)
{
	int prev_val;
	prev_val = gpio_get_value(CAMERA_FLASH_ACT_GPIO);
	gpio_set_value(CAMERA_FLASH_ACT_GPIO, val);
	return prev_val;
};

static struct ssl3250a_platform_data ventana_ssl3250a_data = {
	.config		= CAMERA_FLASH_OP_MODE,
	.max_amp_indic	= CAMERA_FLASH_MAX_LED_AMP,
	.max_amp_torch	= CAMERA_FLASH_MAX_TORCH_AMP,
	.max_amp_flash	= CAMERA_FLASH_MAX_FLASH_AMP,
	.init		= ventana_ssl3250a_init,
	.exit		= ventana_ssl3250a_exit,
	.gpio_act	= ventana_ssl3250a_gpio_act,
	.gpio_en1	= NULL,
	.gpio_en2	= NULL,
	.gpio_strb	= ventana_ssl3250a_gpio_strb,
};
#endif /* CONFIG_TORCH_SSL3250A */

static void ventana_al3000a_init(void)
{
	tegra_gpio_enable(AL3000A_IRQ_GPIO);
	gpio_request(AL3000A_IRQ_GPIO, "al3000a_ls");
	gpio_direction_input(AL3000A_IRQ_GPIO);
}

#ifdef CONFIG_SENSORS_AK8975
static void ventana_akm8975_init(void)
{
	tegra_gpio_enable(AKM8975_IRQ_GPIO);
	gpio_request(AKM8975_IRQ_GPIO, "akm8975");
	gpio_direction_input(AKM8975_IRQ_GPIO);
}
#endif

static void ventana_bq20z75_init(void)
{
	tegra_gpio_enable(AC_PRESENT_GPIO);
	gpio_request(AC_PRESENT_GPIO, "ac_present");
	gpio_direction_input(AC_PRESENT_GPIO);
}

static void ventana_nct1008_init(void)
{
	tegra_gpio_enable(NCT1008_THERM2_GPIO);
	gpio_request(NCT1008_THERM2_GPIO, "temp_alert");
	gpio_direction_input(NCT1008_THERM2_GPIO);
}

static struct nct1008_platform_data ventana_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = false,
	.conv_rate = 0x04,
	.offset = 0,
	.hysteresis = 0,
	.shutdown_ext_limit = 85,
	.shutdown_local_limit = 90,
	.throttling_ext_limit = 65,
	.alarm_fn = tegra_throttling_enable,
};

static const struct i2c_board_info ventana_i2c0_board_info[] = {
	{
		I2C_BOARD_INFO("al3000a_ls", 0x1C),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PZ2),
	},
};

static const struct i2c_board_info ventana_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO("EC_Battery", 0x58),
		.irq = TEGRA_GPIO_TO_IRQ(AC_PRESENT_GPIO),
	},
};

static struct pca953x_platform_data ventana_tca6416_data = {
	.gpio_base      = TEGRA_NR_GPIOS + 4, /* 4 gpios are already requested by tps6586x */
};

static struct pca954x_platform_mode ventana_pca9546_modes[] = {
	{ .adap_id = 6, .deselect_on_exit = 1 }, /* REAR CAM1 */
	{ .adap_id = 7, .deselect_on_exit = 1 }, /* REAR CAM2 */
	{ .adap_id = 8, .deselect_on_exit = 1 }, /* FRONT CAM3 */
};

static struct pca954x_platform_data ventana_pca9546_data = {
	.modes	  = ventana_pca9546_modes,
	.num_modes      = ARRAY_SIZE(ventana_pca9546_modes),
};

static const struct i2c_board_info ventana_i2c3_board_info_tca6416[] = {
	{
		I2C_BOARD_INFO("tca6416", 0x20),
		.platform_data = &ventana_tca6416_data,
	},
};

static const struct i2c_board_info ventana_i2c3_board_info_pca9546[] = {
	{
		I2C_BOARD_INFO("pca9546", 0x70),
		.platform_data = &ventana_pca9546_data,
	},
};

#ifdef CONFIG_TORCH_SSL3250A
static const struct i2c_board_info ventana_i2c3_board_info_ssl3250a[] = {
	{
		I2C_BOARD_INFO("ssl3250a", 0x30),
		.platform_data = &ventana_ssl3250a_data,
	},
};
#endif /* CONFIG_TORCH_SSL3250A */

#if defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)  \
	|| defined(CONFIG_MACH_ACER_VANGOGH)
static struct i2c_board_info ventana_i2c3_board_info[] = {
#ifdef CONFIG_VIDEO_OV5650
	{
		I2C_BOARD_INFO("ov5650", 0x36),
		.platform_data = &ov5650_data,
	},
#endif
#ifdef CONFIG_VIDEO_YUV
	{
		I2C_BOARD_INFO("mt9d115", 0x3C),
		.platform_data = &yuv_sensor_data,
	},
#endif
#ifdef CONFIG_VIDEO_YUV5
	{
		I2C_BOARD_INFO("mt9p111", 0x3D),
		.platform_data = &yuv5_sensor_data,
	},
#endif
#ifdef CONFIG_VIDEO_AD5820
	{
		I2C_BOARD_INFO("ad5820", 0x0C),
	},
#endif
#ifdef CONFIG_VIDEO_LTC3216
	{
		I2C_BOARD_INFO("ltc3216", 0x33),
	},
#endif
};
#endif /* PICASSO || MAYA || VANGOGH */

static struct i2c_board_info ventana_i2c4_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.irq = TEGRA_GPIO_TO_IRQ(NCT1008_THERM2_GPIO),
		.platform_data = &ventana_nct1008_pdata,
	},

#ifdef CONFIG_SENSORS_AK8975
	{
		I2C_BOARD_INFO("akm8975", 0x0C),
		.irq = TEGRA_GPIO_TO_IRQ(AKM8975_IRQ_GPIO),
	},
#endif
};

#if !(defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)  \
	|| defined(CONFIG_MACH_ACER_VANGOGH))
static struct i2c_board_info ventana_i2c6_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650R", 0x36),
		.platform_data = &ventana_right_ov5650_data,
	},
};

static struct i2c_board_info ventana_i2c7_board_info[] = {
	{
		I2C_BOARD_INFO("ov5650L", 0x36),
		.platform_data = &ventana_left_ov5650_data,
	},
	{
		I2C_BOARD_INFO("sh532u", 0x72),
	},
};
#endif /* !(PICASSO || MAYA || VANGOGH) */

#ifdef CONFIG_VIDEO_OV2710
static struct i2c_board_info ventana_i2c8_board_info[] = {
	{
		I2C_BOARD_INFO("ov2710", 0x36),
		.platform_data = &ventana_ov2710_data,
	},
};
#endif

#ifdef CONFIG_MPU_SENSORS_MPU3050
#define SENSOR_MPU_NAME "mpu3050"
static struct mpu3050_platform_data mpu3050_data = {
	.int_config  = 0x10,
#if defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)
	.orientation = {
		 0, -1,  0,
		-1,  0,  0,
		 0,  0, -1
	},
#endif
#ifdef CONFIG_MACH_ACER_VANGOGH
	.orientation = {
		 0, -1,  0,
		 1,  0,  0,
		 0,  0,  1
	},
#endif

	.level_shifter = 0,
	.accel = {
#ifdef CONFIG_MPU_SENSORS_KXTF9
	.get_slave_descr = get_accel_slave_descr,
#else
	.get_slave_descr = NULL,
#endif
	.adapt_num   = 0,
	.irq         = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS7),
	.bus         = EXT_SLAVE_BUS_SECONDARY,
	.address     = 0x0F,
#if defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)
		.orientation = {
			 0, -1,  0,
			-1,  0,  0,
			 0,  0, -1
		},
#endif
#ifdef CONFIG_MACH_ACER_VANGOGH
		.orientation = {
			 0,  1,  0,
			-1,  0,  0,
			 0,  0,  1
		},
#endif
	},

	.compass = {
#ifdef CONFIG_MPU_SENSORS_AK8975
	.get_slave_descr = get_compass_slave_descr,
#else
	.get_slave_descr = NULL,
#endif
	.adapt_num   = 4,            /* bus number 4 on ventana */
	.irq         = TEGRA_GPIO_TO_IRQ(AKM8975_IRQ_GPIO),
	.bus         = EXT_SLAVE_BUS_PRIMARY,
	.address     = 0x0C,
#if defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)
		.orientation = {
			1,  0,  0,
			0, -1,  0,
			0,  0, -1
		},
#endif
#ifdef CONFIG_MACH_ACER_VANGOGH
		.orientation = {
			0, -1,  0,
		       -1,  0,  0,
			0,  0, -1
		},
#endif
	},
};

static struct i2c_board_info __initdata mpu3050_i2c0_boardinfo[] = {
	{
		I2C_BOARD_INFO(SENSOR_MPU_NAME, 0x68),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PZ4),
		.platform_data = &mpu3050_data,
	},
};

static void ventana_mpuirq_init(void)
{
	pr_info("*** MPU START *** ventana_mpuirq_init...\n");
	tegra_gpio_enable(TEGRA_GPIO_PZ4);
	gpio_request(TEGRA_GPIO_PZ4, SENSOR_MPU_NAME);
	gpio_direction_input(TEGRA_GPIO_PZ4);

	tegra_gpio_enable(TEGRA_GPIO_PS7);
	gpio_request(TEGRA_GPIO_PS7, "mpu_kxtf9");
	gpio_direction_input(TEGRA_GPIO_PS7);

	tegra_gpio_enable(AKM8975_IRQ_GPIO);
	gpio_request(AKM8975_IRQ_GPIO, "mpu_akm8975");
	gpio_direction_input(AKM8975_IRQ_GPIO);

	pr_info("*** MPU END *** ventana_mpuirq_init...\n");
}
#endif

int __init ventana_sensors_init(void)
{
	struct board_info BoardInfo;

	ventana_al3000a_init();
#ifdef CONFIG_SENSORS_AK8975
	ventana_akm8975_init();
#endif
#ifdef CONFIG_MPU_SENSORS_MPU3050
	ventana_mpuirq_init();
#endif
	ventana_camera_init();
	ventana_nct1008_init();

	i2c_register_board_info(0, ventana_i2c0_board_info,
		ARRAY_SIZE(ventana_i2c0_board_info));

	tegra_get_board_info(&BoardInfo);

	/*
	 * battery driver is supported on FAB.D boards and above only,
	 * since they have the necessary hardware rework
	 */
	if (BoardInfo.sku > 0) {
		ventana_bq20z75_init();
		i2c_register_board_info(2, ventana_i2c2_board_info,
			ARRAY_SIZE(ventana_i2c2_board_info));
	}

#ifdef CONFIG_TORCH_SSL3250A
	i2c_register_board_info(3, ventana_i2c3_board_info_ssl3250a,
		ARRAY_SIZE(ventana_i2c3_board_info_ssl3250a));
#endif

#if defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)  \
	|| defined(CONFIG_MACH_ACER_VANGOGH)
	i2c_register_board_info(3, ventana_i2c3_board_info,
		ARRAY_SIZE(ventana_i2c3_board_info));
#endif

	i2c_register_board_info(4, ventana_i2c4_board_info,
		ARRAY_SIZE(ventana_i2c4_board_info));

#if !(defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)  \
	|| defined(CONFIG_MACH_ACER_VANGOGH))
	i2c_register_board_info(6, ventana_i2c6_board_info,
		ARRAY_SIZE(ventana_i2c6_board_info));

	i2c_register_board_info(7, ventana_i2c7_board_info,
		ARRAY_SIZE(ventana_i2c7_board_info));
#endif

#ifdef CONFIG_VIDEO_OV2710
	i2c_register_board_info(8, ventana_i2c8_board_info,
		ARRAY_SIZE(ventana_i2c8_board_info));
#endif


#ifdef CONFIG_MPU_SENSORS_MPU3050
	i2c_register_board_info(0, mpu3050_i2c0_boardinfo,
		ARRAY_SIZE(mpu3050_i2c0_boardinfo));
#endif

	return 0;
}

#if !(defined(CONFIG_MACH_ACER_PICASSO) || defined(CONFIG_MACH_ACER_MAYA)  \
	|| defined(CONFIG_MACH_ACER_VANGOGH))
#ifdef CONFIG_TEGRA_CAMERA

struct tegra_camera_gpios {
	const char *name;
	int gpio;
	int enabled;
};

#define TEGRA_CAMERA_GPIO(_name, _gpio, _enabled)		\
	{						\
		.name = _name,				\
		.gpio = _gpio,				\
		.enabled = _enabled,			\
	}

static struct tegra_camera_gpios ventana_camera_gpio_keys[] = {
	[0] = TEGRA_CAMERA_GPIO("en_avdd_csi", AVDD_DSI_CSI_ENB_GPIO, 1),
	[1] = TEGRA_CAMERA_GPIO("cam_i2c_mux_rst_lo", CAM_I2C_MUX_RST_GPIO, 1),

	[2] = TEGRA_CAMERA_GPIO("cam2_ldo_shdn_lo", CAM2_LDO_SHUTDN_L_GPIO, 0),
	[3] = TEGRA_CAMERA_GPIO("cam2_af_pwdn_lo", CAM2_AF_PWR_DN_L_GPIO, 0),
	[4] = TEGRA_CAMERA_GPIO("cam2_pwdn", CAM2_PWR_DN_GPIO, 0),
	[5] = TEGRA_CAMERA_GPIO("cam2_rst_lo", CAM2_RST_L_GPIO, 1),

	[6] = TEGRA_CAMERA_GPIO("cam3_ldo_shdn_lo", CAM3_LDO_SHUTDN_L_GPIO, 0),
	[7] = TEGRA_CAMERA_GPIO("cam3_af_pwdn_lo", CAM3_AF_PWR_DN_L_GPIO, 0),
	[8] = TEGRA_CAMERA_GPIO("cam3_pwdn", CAM3_PWR_DN_GPIO, 0),
	[9] = TEGRA_CAMERA_GPIO("cam3_rst_lo", CAM3_RST_L_GPIO, 1),

	[10] = TEGRA_CAMERA_GPIO("cam1_ldo_shdn_lo", CAM1_LDO_SHUTDN_L_GPIO, 0),
	[11] = TEGRA_CAMERA_GPIO("cam1_af_pwdn_lo", CAM1_AF_PWR_DN_L_GPIO, 0),
	[12] = TEGRA_CAMERA_GPIO("cam1_pwdn", CAM1_PWR_DN_GPIO, 0),
	[13] = TEGRA_CAMERA_GPIO("cam1_rst_lo", CAM1_RST_L_GPIO, 1),
};

int __init ventana_camera_late_init(void)
{
	int ret;
	int i;
	struct regulator *cam_ldo6 = NULL;

	if (!machine_is_ventana())
		return 0;

	cam_ldo6 = regulator_get(NULL, "vdd_ldo6");
	if (IS_ERR_OR_NULL(cam_ldo6)) {
		pr_err("%s: Couldn't get regulator ldo6\n", __func__);
		return PTR_ERR(cam_ldo6);
	}

	ret = regulator_set_voltage(cam_ldo6, 1800*1000, 1800*1000);
	if (ret){
		pr_err("%s: Failed to set ldo6 to 1.8v\n", __func__);
		goto fail_put_regulator;
	}

	ret = regulator_enable(cam_ldo6);
	if (ret){
		pr_err("%s: Failed to enable ldo6\n", __func__);
		goto fail_put_regulator;
	}

	i2c_new_device(i2c_get_adapter(3), ventana_i2c3_board_info_tca6416);

	for (i = 0; i < ARRAY_SIZE(ventana_camera_gpio_keys); i++) {
		ret = gpio_request(ventana_camera_gpio_keys[i].gpio,
			ventana_camera_gpio_keys[i].name);
		if (ret < 0) {
			pr_err("%s: gpio_request failed for gpio #%d\n",
				__func__, i);
			goto fail_free_gpio;
		}
		gpio_direction_output(ventana_camera_gpio_keys[i].gpio,
			ventana_camera_gpio_keys[i].enabled);
		gpio_export(ventana_camera_gpio_keys[i].gpio, false);
	}

	i2c_new_device(i2c_get_adapter(3), ventana_i2c3_board_info_pca9546);

	ventana_ov2710_power_off();
	ventana_left_ov5650_power_off();
	ventana_right_ov5650_power_off();

	ret = regulator_disable(cam_ldo6);
	if (ret){
		pr_err("%s: Failed to disable ldo6\n", __func__);
		goto fail_free_gpio;
	}

	regulator_put(cam_ldo6);
	return 0;

fail_free_gpio:
	while (i--)
		gpio_free(ventana_camera_gpio_keys[i].gpio);

fail_put_regulator:
	regulator_put(cam_ldo6);
	return ret;
}

late_initcall(ventana_camera_late_init);

#endif /* CONFIG_TEGRA_CAMERA */
#endif /* !(PICASSO || MAYA || VANGOGH) */
