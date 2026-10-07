#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "animation.h"
#include "gfx.h"
#include "temperature.h"
#include "web_server.h"
#include "wifi_ap.h"

#define FRAME_MS 30

int main(void)
{
	int64_t next_frame, hold_until;
	int32_t hold_ms;
	int ret;

	ret = gfx_init();
	if (ret < 0) {
		printk("Display init failed: %d\n", ret);
		return 0;
	}

	ret = temperature_init();
	if (ret < 0) {
		printk("DHT sensor is not ready: %d\n", ret);
	}

	ret = wifi_ap_start();
	if (ret < 0) {
		printk("Wi-Fi AP start failed: %d\n", ret);
	}

	ret = web_server_start();
	if (ret < 0) {
		printk("Web server start failed: %d\n", ret);
	}

	next_frame = k_uptime_get();
	hold_until = 0;

	while (1) {
		temperature_poll();

		/*
		 * While an animation holds a frame (e.g. Pong after a point), keep
		 * the screen as it is, but end the hold early on a switch request.
		 */
		if (k_uptime_get() >= hold_until || animation_switch_pending()) {
			gfx_clear();
			hold_ms = animation_run_frame();
			temperature_draw();

			ret = gfx_flush();
			if (ret < 0) {
				printk("Display write failed: %d\n", ret);
			}

			if (hold_ms > 0) {
				hold_until = k_uptime_get() + hold_ms;
				next_frame = k_uptime_get();
			}
		}

		next_frame += FRAME_MS;
		k_sleep(K_TIMEOUT_ABS_MS(next_frame));
	}

	return 0;
}
