#include "gps.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(gps);

static struct device *gps_dev = NULL;

int gps_control_init(gps_trigger_handler_t handler)
{
	int err;

	struct gps_trigger gps_trig = {
		.type = GPS_TRIG_FIX,
		.chan = GPS_CHAN_PVT
	};

	gps_dev = device_get_binding("NRF9160_GPS");
	if (gps_dev == NULL) {
		LOG_ERR("Could not get device");
		return -ENODEV;
	}

	err = gps_trigger_set(gps_dev, &gps_trig, handler);
	if (err) {
		LOG_ERR("Could not set trigger, error code: %d", err);
		return err;
	}

	LOG_INF("GPS initialized");

	return 0;
}

int gps_control_start(void) {
    int err = gps_start(gps_dev);
    if (err) {
        LOG_WRN("Error starting GPS: %d", err);
    }

    return err;
}


int gps_control_stop(void)
{
	int err;

    LOG_INF("Disabling PSM");

    err = lte_lc_psm_req(false);
    if (err) {
        LOG_WRN("Unable to disable PSM: %d", err);
    }
    k_sleep(K_SECONDS(1));

    if (!gps_dev) {
        LOG_WRN("Unable to get GPS device");
        return -ENODEV;
    }

	err = gps_stop(gps_dev);
	if (err) {
        LOG_WRN("Unable to stop GPS: %d", err);
		return err;
	}

	return 0;
}
