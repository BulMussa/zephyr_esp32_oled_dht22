#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include "animation.h"

static const struct animation *const animations[ANIMATION_COUNT] = {
	[ANIMATION_PONG] = &pong_animation,
	[ANIMATION_SINE] = &sine_animation,
};

/* Written by the web server thread, read by the display loop. */
static atomic_t requested = ATOMIC_INIT(IS_ENABLED(CONFIG_APP_ANIMATION_SINE) ? ANIMATION_SINE
									      : ANIMATION_PONG);

/* Only touched by the display loop; -1 until the first frame. */
static int current = -1;

void animation_request(enum animation_id id)
{
	if (id >= 0 && id < ANIMATION_COUNT) {
		atomic_set(&requested, id);
	}
}

enum animation_id animation_requested(void)
{
	return (enum animation_id)atomic_get(&requested);
}

bool animation_switch_pending(void)
{
	return atomic_get(&requested) != current;
}

const char *animation_name(enum animation_id id)
{
	if (id < 0 || id >= ANIMATION_COUNT) {
		return NULL;
	}

	return animations[id]->name;
}

int animation_find(const char *name, size_t len)
{
	for (int i = 0; i < ANIMATION_COUNT; i++) {
		if (strlen(animations[i]->name) == len &&
		    strncmp(animations[i]->name, name, len) == 0) {
			return i;
		}
	}

	return -EINVAL;
}

int32_t animation_run_frame(void)
{
	int next = atomic_get(&requested);

	if (next != current) {
		current = next;
		animations[current]->init();
	}

	return animations[current]->step();
}
