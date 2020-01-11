#include <zephyr.h>
#include <sensor.h>

#include "activity_tracker.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(act);


static const char* ACCEL_DEVICE_NAME="ADXL362";

static void sensor_trigger_handler(struct device *dev,
			struct sensor_trigger *trigger)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trigger);

	LOG_INF("Motion detected");
}

int init_activity_tracker(void) {
    struct device *accel_dev = device_get_binding(ACCEL_DEVICE_NAME);
    if (!accel_dev) {
        LOG_ERR("Unable to initialize %s", ACCEL_DEVICE_NAME);
        return -ENODEV;
    }

    struct sensor_trigger sensor_trig = {
        .type = SENSOR_TRIG_THRESHOLD,
    };

    int err = sensor_trigger_set(accel_dev, &sensor_trig, sensor_trigger_handler);
    if (err) {
        LOG_ERR("Unable to configure %s trigger: %d", ACCEL_DEVICE_NAME, err);
        return err;
    }

    return 0;
}