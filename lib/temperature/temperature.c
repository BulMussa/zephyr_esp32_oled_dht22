#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include "gfx.h"
#include "temperature.h"

#define DHT_FIRST_READ_MS 2000
#define DHT_INTERVAL_MS   5000

#define TEXT_SCALE 2
#define TEXT_Y     3

static const struct device *const dht = DEVICE_DT_GET(DT_NODELABEL(dht22));

static bool dht_ok;
static int64_t next_read = DHT_FIRST_READ_MS;

/* Last good reading for temperature_get(); read by the web server thread. */
static atomic_t last_tenths;
static atomic_t have_reading;

/* Temperature text, "--.-" until the first good read. */
static char temp_text[16] = "--.-*C";

/*
 * Read the temperature from the DHT sensor in tenths of a degree Celsius.
 * Returns 0 on success or a negative errno code on failure.
 */
static int dht_read_temp(const struct device *dev, int32_t *temp_tenths)
{
	struct sensor_value temp;
	int ret;

	ret = sensor_sample_fetch(dev);
	if (ret < 0) {
		return ret;
	}

	ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	if (ret < 0) {
		return ret;
	}

	*temp_tenths = temp.val1 * 10 + temp.val2 / 100000;

	return 0;
}

/* Read the sensor and update the temperature text; keeps the old value on failure. */
static void update_temperature(void)
{
	int32_t t;
	int ret;

	ret = dht_read_temp(dht, &t);
	if (ret < 0) {
		printk("Failed to read DHT sensor: %d\n", ret);
		return;
	}

	atomic_set(&last_tenths, t);
	atomic_set(&have_reading, 1);

	snprintf(temp_text, sizeof(temp_text), "%s%d.%d%cC", t < 0 ? "-" : "", abs(t) / 10,
		 abs(t) % 10, GFX_DEGREE_CHAR);
}

int temperature_init(void)
{
	dht_ok = device_is_ready(dht);

	return dht_ok ? 0 : -ENODEV;
}

void temperature_poll(void)
{
	/* The DHT22 needs a moment after power-up before the first reading */
	if (dht_ok && k_uptime_get() >= next_read) {
		update_temperature();
		next_read = k_uptime_get() + DHT_INTERVAL_MS;
	}
}

bool temperature_get(int32_t *tenths)
{
	if (!atomic_get(&have_reading)) {
		return false;
	}

	*tenths = (int32_t)atomic_get(&last_tenths);

	return true;
}

void temperature_draw(void)
{
	gfx_draw_text(SCREEN_W - gfx_text_width(temp_text, TEXT_SCALE), TEXT_Y, temp_text,
		      TEXT_SCALE);
}
