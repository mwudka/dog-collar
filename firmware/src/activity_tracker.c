#include <zephyr.h>
#include <sensor.h>
#include <sys/time.h>
#include <sys/base64.h>

#include "vendor/croaring/roaring.h"

#include "activity_tracker.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(act);


static const char* ACCEL_DEVICE_NAME="ADXL362";

static roaring_bitmap_t *periods_with_activity = NULL;
static time_t start_timestamp;

static int report_activity_history() {
    bool bitmap_optimized = roaring_bitmap_run_optimize(periods_with_activity);
    if (bitmap_optimized) {
        LOG_DBG("Optimized activity bitmap");
    }

    size_t bytes_needed_to_serialize = roaring_bitmap_portable_size_in_bytes(periods_with_activity);
    char* serialized_activity_bitmap = malloc(bytes_needed_to_serialize);
    if (!serialized_activity_bitmap) {
        LOG_WRN("Unable to allocate memory for serialized activity history");
        return -ENOMEM;
    }
    size_t bytes_serialized = roaring_bitmap_portable_serialize(periods_with_activity, serialized_activity_bitmap);
    int ret = 0;
    if (bytes_needed_to_serialize != bytes_serialized) {
        LOG_WRN("Error serializing activity history: serialized data should have used %d bytes, but used %d", bytes_needed_to_serialize, bytes_serialized);
        ret = -EIO;
    }

    size_t base64_size = 0;
    base64_encode(NULL, 0, &base64_size, serialized_activity_bitmap, bytes_serialized);
    LOG_DBG("Activity base64 requires %d bytes", base64_size);
    // TODO: Error handling
    char* base64 = malloc(base64_size);
    if (!base64) {
        LOG_ERR("Unable to allocate memory for serialized activity history");
        ret = -ENOMEM;
    } else {
        base64_encode(base64, base64_size, &base64_size, serialized_activity_bitmap, bytes_serialized);

        LOG_INF("Serialized activity history %s", base64);

        free(base64);
    }
    
    free(serialized_activity_bitmap);

    // TODO: Clear the bitmask after it's uploaded

    return ret;
}

K_MUTEX_DEFINE(my_mutex);

static void sensor_trigger_handler(struct device *dev,
			struct sensor_trigger *trigger)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trigger);

    struct timeval tv;
    int err = gettimeofday(&tv, NULL);
    if (err) {
        LOG_WRN("Unable to get time: %d", err);
        return;
    }
    uint32_t motion_window = (uint32_t)(tv.tv_sec - start_timestamp);

	LOG_INF("Motion detected at %u", motion_window);

    k_mutex_lock(&my_mutex, K_FOREVER);
    // TODO: Use one bit per minute, rather than per second, to use less space
    roaring_bitmap_add(periods_with_activity, motion_window);

    // TODO: only report activity after idle period
    report_activity_history();
    k_mutex_unlock(&my_mutex);

}

int init_activity_tracker(void) {
    struct device *accel_dev = device_get_binding(ACCEL_DEVICE_NAME);
    if (!accel_dev) {
        LOG_ERR("Unable to initialize %s", ACCEL_DEVICE_NAME);
        return -ENODEV;
    }

    struct timeval tv;
    int err = gettimeofday(&tv, NULL);
    if (err) {
        LOG_ERR("Unable to get time: %d", err);
        return err;
    }
    start_timestamp = (uint64_t)tv.tv_sec;

    periods_with_activity = roaring_bitmap_create();
    if (!periods_with_activity) {
        LOG_ERR("Unable to create activity bitset");
        // TODO: What can cause this to fail?
        return -ENOMEM;
    }

    // TODO: Switch to ADXL372 with lower threshold for improved sensitivity

    struct sensor_trigger sensor_trig = {
        .type = SENSOR_TRIG_THRESHOLD,
    };

    err = sensor_trigger_set(accel_dev, &sensor_trig, sensor_trigger_handler);
    if (err) {
        LOG_ERR("Unable to configure %s trigger: %d", ACCEL_DEVICE_NAME, err);
        return err;
    }

    return 0;
}