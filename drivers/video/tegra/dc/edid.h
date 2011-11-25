/*
 * drivers/video/tegra/dc/edid.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_EDID_H
#define __DRIVERS_VIDEO_TEGRA_DC_EDID_H

#include <linux/i2c.h>
#include <linux/wait.h>

struct tegra_edid {
	struct i2c_client       *client;
	struct i2c_board_info   info;
	int                     bus;

	u8          *data;
	unsigned    len;
	u8          support_stereo;
	u8          vsdb;
};

struct tegra_edid *tegra_edid_create(int bus);
void tegra_edid_destroy(struct tegra_edid *edid);

int tegra_edid_get_monspecs(struct tegra_edid *edid, struct fb_monspecs *specs);

#endif
