#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#include <stdbool.h>
#include <stdint.h>

/* Check the DHT22 sensor. Returns 0 or a negative errno code. */
int temperature_init(void);

/* Read the sensor if the read interval has passed. Blocks ~25 ms when it reads. */
void temperature_poll(void);

/*
 * Get the last good temperature in tenths of a degree Celsius. Safe to call
 * from any thread. Returns false if there has been no good reading yet.
 */
bool temperature_get(int32_t *tenths);

/* Draw the last temperature right-aligned in the upper right corner. */
void temperature_draw(void);

#endif /* TEMPERATURE_H_ */
