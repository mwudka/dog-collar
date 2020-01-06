#include <zephyr.h>
#include <lte_lc.h>
#include <modem_info.h>

#include <bluetooth/services/bas.h>
#include "ble.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(dog);


static struct modem_param_info modem_param;

void main(void)
{

    LOG_INF("Application started");

	ble_init();

    LOG_INF("Connecting to LTE...");
    int err = lte_lc_init_and_connect();
	__ASSERT(err == 0, "LTE link could not be established.");
    LOG_INF("Connected to LTE");

    LOG_INF("Initializing modem info...");
    err = modem_info_init();
    __ASSERT(err == 0, "Modem info could not be inited.");
    LOG_INF("Modem info inited");

    modem_info_params_init(&modem_param);
    
    while (true) {
        LOG_INF("Main app loop");



        int ret = modem_info_params_get(&modem_param);
        if (ret < 0) {
		    LOG_WRN("Unable to obtain modem parameters: %d", ret);
        } else {
            LOG_INF("Battery voltage: %u", modem_param.device.battery.value);
            // TODO: Does the battery really go up to 4.4V?
            uint16_t battery_percent = 100 * modem_param.device.battery.value / 4406;
            if (battery_percent > 100) {
                battery_percent = 100;
            }
            bt_gatt_bas_set_battery_level(battery_percent);
        }

		k_sleep(K_SECONDS(1));
		k_cpu_idle();
	}
}