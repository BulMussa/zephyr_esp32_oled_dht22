#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/util.h>

#include "gfx.h"

static const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* SSD1306 layout: each byte holds 8 vertical pixels, LSB on top. */
static uint8_t fb[SCREEN_W * SCREEN_H / 8];

/* 3x5 digit font, one byte per row, 3 LSBs used. */
static const uint8_t digits[10][5] = {
	{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
	{7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

/* Extra glyphs for text: width in columns and 3x5 rows. */
struct glyph {
	char c;
	uint8_t width;
	uint8_t rows[5];
};

static const struct glyph symbols[] = {
	{'-', 3, {0, 0, 7, 0, 0}},
	{'.', 1, {0, 0, 0, 0, 1}},
	{GFX_DEGREE_CHAR, 3, {7, 5, 7, 0, 0}},
	{'C', 3, {7, 4, 4, 4, 7}},
};

int gfx_init(void)
{
	int ret;

	if (!device_is_ready(display)) {
		return -ENODEV;
	}

	ret = display_set_pixel_format(display, PIXEL_FORMAT_MONO01);
	if (ret < 0) {
		return ret;
	}

	return display_blanking_off(display);
}

void gfx_clear(void)
{
	memset(fb, 0, sizeof(fb));
}

int gfx_flush(void)
{
	struct display_buffer_descriptor desc = {
		.buf_size = sizeof(fb),
		.width = SCREEN_W,
		.height = SCREEN_H,
		.pitch = SCREEN_W,
	};

	return display_write(display, 0, 0, &desc, fb);
}

void gfx_set_pixel(int x, int y)
{
	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) {
		return;
	}

	fb[(y / 8) * SCREEN_W + x] |= BIT(y % 8);
}

void gfx_fill_rect(int x, int y, int w, int h)
{
	for (int j = y; j < y + h; j++) {
		for (int i = x; i < x + w; i++) {
			gfx_set_pixel(i, j);
		}
	}
}

/* Bresenham line from (x0, y0) to (x1, y1), both ends included. */
void gfx_draw_line(int x0, int y0, int x1, int y1)
{
	int dx = abs(x1 - x0);
	int dy = -abs(y1 - y0);
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;

	while (true) {
		gfx_set_pixel(x0, y0);
		if (x0 == x1 && y0 == y1) {
			break;
		}

		int e2 = 2 * err;

		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

static void draw_glyph(int x, int y, uint8_t width, const uint8_t *rows, int scale)
{
	for (int row = 0; row < 5; row++) {
		for (int col = 0; col < width; col++) {
			if (rows[row] & BIT(width - 1 - col)) {
				gfx_fill_rect(x + col * scale, y + row * scale, scale, scale);
			}
		}
	}
}

void gfx_draw_digit(int x, int y, int digit, int scale)
{
	draw_glyph(x, y, 3, digits[digit % 10], scale);
}

/* Find the glyph for a character; digits map onto the digit font. */
static bool get_glyph(char c, uint8_t *width, const uint8_t **rows)
{
	if (c >= '0' && c <= '9') {
		*width = 3;
		*rows = digits[c - '0'];
		return true;
	}

	for (size_t i = 0; i < ARRAY_SIZE(symbols); i++) {
		if (symbols[i].c == c) {
			*width = symbols[i].width;
			*rows = symbols[i].rows;
			return true;
		}
	}

	return false;
}

int gfx_text_width(const char *str, int scale)
{
	const uint8_t *rows;
	uint8_t width;
	int total = 0;

	for (; *str != '\0'; str++) {
		if (get_glyph(*str, &width, &rows)) {
			total += (width + 1) * scale;
		}
	}

	return total > 0 ? total - scale : 0;
}

void gfx_draw_text(int x, int y, const char *str, int scale)
{
	const uint8_t *rows;
	uint8_t width;

	for (; *str != '\0'; str++) {
		if (!get_glyph(*str, &width, &rows)) {
			continue;
		}

		draw_glyph(x, y, width, rows, scale);
		x += (width + 1) * scale;
	}
}
