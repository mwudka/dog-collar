#include <zephyr.h>

#include "ble.h"

void main(void)
{

    printk("Application started\n");

	ble_init();
    
    while (true) {
        printk("Main app loop\n");
		k_sleep(K_SECONDS(1));
		k_cpu_idle();
	}
}