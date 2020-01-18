#include <zephyr.h>
#include <sensor.h>
#include <sys/time.h>

#include "vendor/croaring/roaring.h"

#include "activity_tracker.h"
#include "server_upload.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(act);


static const char* ACCEL_DEVICE_NAME="ADXL362";

static roaring_bitmap_t *periods_with_activity = NULL;
static time_t start_timestamp;
static bool activity_since_upload = true;

K_MUTEX_DEFINE(my_mutex);

#define ACTIVITY_UPLOAD_STACK_SIZE (4096)
K_THREAD_STACK_DEFINE(activity_upload_stack_area, ACTIVITY_UPLOAD_STACK_SIZE);
struct k_work_q activity_upload_q;
struct k_delayed_work activity_uplaod_work;

static const int UPLOAD_INTERVAL = K_SECONDS(5);

static void upload_activity(struct k_work *item) {
    LOG_INF("Uploading activity info");

    k_mutex_lock(&my_mutex, K_FOREVER);

    if (activity_since_upload) {
        activity_since_upload = false;

        report_activity_history(start_timestamp, periods_with_activity);
    }

    k_mutex_unlock(&my_mutex);

    k_delayed_work_submit_to_queue(&activity_upload_q, &activity_uplaod_work, UPLOAD_INTERVAL);
}

static void activity_detected() {
    uint32_t motion_window = (uint32_t)(k_uptime_get() / 1000 - start_timestamp);

	LOG_INF("Motion detected at %u", motion_window);

    k_mutex_lock(&my_mutex, K_FOREVER);

    activity_since_upload = true;

    // TODO: Use one bit per minute, rather than per second, to use less space
    roaring_bitmap_add(periods_with_activity, motion_window);

    k_mutex_unlock(&my_mutex);
}

static void sensor_trigger_handler(struct device *dev,
			struct sensor_trigger *trigger)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trigger);

    activity_detected();
}

int init_activity_tracker(void) {
    int err = 0;

    start_timestamp = k_uptime_get() / 1000;

    periods_with_activity = roaring_bitmap_create();
    if (!periods_with_activity) {
        LOG_ERR("Unable to create activity bitset");
        // TODO: What can cause this to fail?
        return -ENOMEM;
    }

    struct device *accel_dev = device_get_binding(ACCEL_DEVICE_NAME);
    if (!accel_dev) {
        LOG_ERR("Unable to initialize %s; creating virtual accel", ACCEL_DEVICE_NAME);

        // TODO: Dispatch on a schedule to simulate fun activity
        activity_detected();

        return -ENODEV;
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

    k_delayed_work_init(&activity_uplaod_work, upload_activity);
    
    k_work_q_start(&activity_upload_q, activity_upload_stack_area, ACTIVITY_UPLOAD_STACK_SIZE, 1);

    k_delayed_work_submit_to_queue(&activity_upload_q, &activity_uplaod_work, UPLOAD_INTERVAL);

    return 0;
}