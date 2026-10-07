#ifndef GFX_H_
#define GFX_H_

#define SCREEN_W 128
#define SCREEN_H 64

/*
 * Two-color module: rows 0-15 are yellow (info area: score, temperature),
 * rows 16-63 are blue (animation area).
 */
#define INFO_AREA_H 16

/* Set the pixel format and turn the display on. Returns 0 or a negative errno code. */
int gfx_init(void);

/* Clear the frame buffer. */
void gfx_clear(void);

/* Send the frame buffer to the display. Returns 0 or a negative errno code. */
int gfx_flush(void);

void gfx_set_pixel(int x, int y);
void gfx_fill_rect(int x, int y, int w, int h);
void gfx_draw_line(int x0, int y0, int x1, int y1);

/* Draw a digit from the 3x5 font scaled by 'scale'. */
void gfx_draw_digit(int x, int y, int digit, int scale);

/*
 * Text with the 3x5 font: digits, '-', '.', 'C' and GFX_DEGREE_CHAR
 * (drawn as a degree sign). Other characters are skipped.
 */
#define GFX_DEGREE_CHAR '*'

int gfx_text_width(const char *str, int scale);
void gfx_draw_text(int x, int y, const char *str, int scale);

#endif /* GFX_H_ */
