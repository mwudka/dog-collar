/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdbool.h>
#include <string.h>
#include <zephyr.h>
#include <zephyr/types.h>

#include <drivers/adp536x.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(batt);

// TODO: This should pull from a magical constant like DT_NORDIC_NRF_I2C_I2C_2_LABEL
#define ADP536X_I2C_DEV_NAME	        "I2C_2"
#define BATTERY_CHARGE_CAPACITY         0xFF /* Maximum battery capacity. */
#define BATTERY_CHARGE_SLEEP_MODE       true /* Set sleep mode. */

// TODO: Consider increasing/dynamically changing the update rate if it turns out that
// the frequent updates use too much power
#define BATTERY_CHARGE_UPDATE_RATE      0 /* Sample every minute. */

int battery_monitor_init(void)
{
	int ret;

	ret = adp536x_init(ADP536X_I2C_DEV_NAME);
	if (ret) {
        LOG_WRN("Unable to init ADP536X driver: %d", ret);
		return ret;
	}

	ret = adp536x_bat_cap_set(BATTERY_CHARGE_CAPACITY);
	if (ret) {
		LOG_WRN("Failed to set battery capacity: %d", ret);
		return ret;
	}

	ret = adp536x_fuel_gauge_enable_sleep_mode(BATTERY_CHARGE_SLEEP_MODE);
	if (ret) {
		LOG_WRN("Failed to enable fuel gauge sleep mode: %d", ret);
		return ret;
	}

	ret = adp536x_fuel_gauge_update_rate_set(BATTERY_CHARGE_UPDATE_RATE);
	if (ret) {
		LOG_WRN("Failed to set fuel gauge update rate: %d", ret);
		return ret;
	}

	ret = adp536x_fuel_gauge_set(true);
	if (ret) {
		LOG_WRN("Failed to enable fuel gauge: %d", ret);
		return ret;
	}

    return ret;
}

int battery_monitor_read(u8_t *buf)
{
	return adp536x_bat_soc_read(buf);
}