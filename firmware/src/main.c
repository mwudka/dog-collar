#include <zephyr.h>
#include <lte_lc.h>
#include <modem_info.h>
#include <string.h>
#include <data/json.h>

#include <bluetooth/services/bas.h>
#include "ble.h"
#include "gps.h"
#include "battery_monitor.h"
#include "activity_tracker.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(dog);


static struct modem_param_info modem_param;

static struct gps_data gps_data;

static atomic_t gps_is_active;

struct k_delayed_work gps_stop_work;
struct k_delayed_work gps_start_work;

struct gps_fix {
	int lat_millideg;
	int lon_millideg;
    int alt;
    int acc;
};

static struct json_obj_descr gps_fix_desc[] = {
	JSON_OBJ_DESCR_PRIM(struct gps_fix, lat_millideg, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct gps_fix, lon_millideg, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct gps_fix, alt, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct gps_fix, acc, JSON_TOK_NUMBER)
};

char gps_fix_json[100] = {0};


static void gps_trigger_handler(struct device *dev, struct gps_trigger *trigger)
{
    LOG_INF("GPS triggered");
	ARG_UNUSED(trigger);
	
	gps_sample_fetch(dev);
    gps_channel_get(dev, GPS_CHAN_PVT, &gps_data);

    struct gps_fix gps_fix_info = {
        .lat_millideg = (int)(1000 * gps_data.pvt.latitude),
        .lon_millideg = (int)(1000 * gps_data.pvt.longitude),
        .alt = (int)(gps_data.pvt.altitude),
        .acc = (int)(gps_data.pvt.accuracy),
    };

    int res = json_obj_encode_buf(gps_fix_desc, ARRAY_SIZE(gps_fix_desc), &gps_fix_info, gps_fix_json, sizeof(gps_fix_json));
    if (res) {
        LOG_WRN("Error encoding GPS fix JSON: %d", res);
        return;
    }
    
    LOG_INF("Fix info: %s", gps_fix_json);

    k_delayed_work_submit(&gps_stop_work, K_SECONDS(0));
}

static void gps_stop_handler(struct k_work *work) {
    LOG_INF("Stopping GPS");
    int err = gps_control_stop();
    if (err) {
        LOG_INF("Error stopping GPS: %d", err);
    } else {
        LOG_INF("GPS Stopped");
    }

    atomic_set(&gps_is_active, 0);

    LOG_INF("Sending fix to server: %s", gps_fix_json);

    k_delayed_work_submit(&gps_start_work, K_SECONDS(10));
}

static void gps_start_handler(struct k_work *work) {
    LOG_INF("Starting GPS");
    atomic_set(&gps_is_active, 1);

    LOG_INF("Enabling PSM");
    int err = lte_lc_psm_req(true);
    if (err) {
        LOG_WRN("PSM mode could not be enabled or was already enabled.");
    } else {
        LOG_INF("PSM enabled\n");
    }
    k_sleep(K_SECONDS(1));

    err = gps_control_start();
    if (err) {
        LOG_WRN("Error starting GPS: %d", err);
    }

    k_delayed_work_submit(&gps_stop_work, K_SECONDS(600));
}


void main(void)
{
    LOG_INF("Application started");

	ble_init();

    int err = battery_monitor_init();
    if (err) {
        LOG_WRN("Error initializing bttery monitor: %d", err);
    }

    LOG_INF("Connecting to LTE...");
    err = lte_lc_init_and_connect();
	__ASSERT(err == 0, "LTE link could not be established.");
    LOG_INF("Connected to LTE");

    LOG_INF("Initializing modem info...");
    err = modem_info_init();
    __ASSERT(err == 0, "Modem info could not be inited.");
    LOG_INF("Modem info inited");

    modem_info_params_init(&modem_param);

    LOG_INF("Initializing GPS...");
    gps_control_init(gps_trigger_handler);
    LOG_INF("GPS inited");

    LOG_INF("Initializing activity tracker...");
    err = init_activity_tracker();
    if (err) {
        LOG_WRN("Error initing activity tracker: %d", err);
    } else {
        LOG_INF("Activity tracker ready");
    }
    
    while (true) {
        LOG_INF("Main app loop");

        u8_t battery_percent;
        int ret = battery_monitor_read(&battery_percent);
        if (ret) {
            LOG_WRN("Unable to read battery level: %d", ret);
        } else {
            LOG_INF("Battery level: %d", battery_percent);
            if (battery_percent < 0) {
                battery_percent = 0;
            }
            if (battery_percent > 100) {
                battery_percent = 100;
            }
            bt_gatt_bas_set_battery_level((uint16_t)battery_percent);
        }

        if (!atomic_get(&gps_is_active)) {
            int ret = modem_info_params_get(&modem_param);
            if (ret < 0) {
                LOG_WRN("Unable to obtain modem parameters: %d", ret);
            } else {
                LOG_INF("Cell tower information: mcc: %03d mnc: %03d area_code=%d cellid=%u", modem_param.network.mcc.value, modem_param.network.mnc.value, modem_param.network.area_code.value, (unsigned int)modem_param.network.cellid_dec);
                LOG_INF("Cell datetime: %s", modem_param.network.date_time.value_string);
            }
        }

		k_sleep(K_SECONDS(1));
	}
}