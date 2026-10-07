#include <math.h>
#include <stdint.h>

#include "animation.h"
#include "gfx.h"

/*
 * Graph in the blue area: border lines on the top and bottom edges, a dotted
 * x axis through the middle with a tick every TICK_SPACING pixels, and a sine
 * wave that slides to the right by WAVE_SPEED pixels per frame.
 */
#define GRAPH_TOP    INFO_AREA_H
#define GRAPH_BOT    (SCREEN_H - 1)
#define AXIS_Y       ((GRAPH_TOP + GRAPH_BOT) / 2)
#define TICK_SPACING 16

/* Peak height in pixels; must stay below (GRAPH_BOT - GRAPH_TOP) / 2. */
#define WAVE_AMPLITUDE 19
/* Length of one full wave in pixels */
#define WAVE_PERIOD    64
/* Pixels the wave moves right per frame (30 ms) */
#define WAVE_SPEED     1

#define TWO_PI 6.28318531f

/* One period of the wave, precomputed so each frame only does table lookups. */
static int8_t wave[WAVE_PERIOD];
static int phase;

/* Height of the wave above the axis at screen column x. */
static int wave_at(int x)
{
	int i = (x - phase) % WAVE_PERIOD;

	if (i < 0) {
		i += WAVE_PERIOD;
	}

	return wave[i];
}

static void draw_axes(void)
{
	gfx_fill_rect(0, GRAPH_TOP, SCREEN_W, 1);
	gfx_fill_rect(0, GRAPH_BOT, SCREEN_W, 1);

	/* Dotted x axis */
	for (int x = 0; x < SCREEN_W; x += 2) {
		gfx_set_pixel(x, AXIS_Y);
	}

	/* Ticks on the x axis */
	for (int x = TICK_SPACING; x < SCREEN_W; x += TICK_SPACING) {
		gfx_fill_rect(x, AXIS_Y - 2, 1, 5);
	}
}

static void sine_init(void)
{
	for (int i = 0; i < WAVE_PERIOD; i++) {
		wave[i] = (int8_t)lroundf(WAVE_AMPLITUDE * sinf(TWO_PI * (float)i / WAVE_PERIOD));
	}

	phase = 0;
}

static int32_t sine_step(void)
{
	int prev_y = AXIS_Y - wave_at(1);

	draw_axes();

	/* Connect neighboring points so steep parts of the wave have no gaps */
	for (int x = 2; x < SCREEN_W; x++) {
		int y = AXIS_Y - wave_at(x);

		gfx_draw_line(x - 1, prev_y, x, y);
		prev_y = y;
	}

	/* Moving the phase forward shifts the whole wave to the right */
	phase = (phase + WAVE_SPEED) % WAVE_PERIOD;

	return 0;
}

const struct animation sine_animation = {
	.name = "sine",
	.init = sine_init,
	.step = sine_step,
};
