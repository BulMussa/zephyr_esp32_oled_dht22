#ifndef ANIMATION_H_
#define ANIMATION_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum animation_id {
	ANIMATION_PONG,
	ANIMATION_SINE,
	ANIMATION_COUNT,
};

struct animation {
	/* Short name used by the web API, e.g. "pong" */
	const char *name;

	/* Reset the animation state. Called every time the animation is switched to. */
	void (*init)(void);

	/*
	 * Advance the animation by one frame and draw it into the cleared frame
	 * buffer. Returns how long (ms) to hold this frame on screen before the
	 * next one, or 0 for the normal frame rate.
	 */
	int32_t (*step)(void);
};

extern const struct animation pong_animation;
extern const struct animation sine_animation;

/*
 * Ask for a different animation. Safe to call from any thread; the display
 * loop switches at its next frame.
 */
void animation_request(enum animation_id id);

/* The animation that is requested (shown from the next frame on). */
enum animation_id animation_requested(void);

/* True while a requested switch has not been applied by the display loop yet. */
bool animation_switch_pending(void);

/* Name of an animation, or NULL for an invalid id. */
const char *animation_name(enum animation_id id);

/* Look up an animation by name. Returns its id or -EINVAL. */
int animation_find(const char *name, size_t len);

/*
 * Display loop only: apply a pending switch, then step and draw the current
 * animation. Returns the hold time from the animation's step().
 */
int32_t animation_run_frame(void);

#endif /* ANIMATION_H_ */
