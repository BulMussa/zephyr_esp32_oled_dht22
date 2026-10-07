#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "animation.h"
#include "gfx.h"

/*
 * The playing field is the blue area, framed by border lines on its top and
 * bottom rows; the ball and paddles move between them.
 */
#define BORDER_TOP INFO_AREA_H
#define BORDER_BOT (SCREEN_H - 1)
#define FIELD_TOP  (BORDER_TOP + 1)
#define FIELD_BOT  BORDER_BOT
#define FIELD_H    (FIELD_BOT - FIELD_TOP)

/* Positions and velocities are fixed point with FP_SHIFT fractional bits. */
#define FP_SHIFT 8
#define FP(x)    ((int32_t)(x) << FP_SHIFT)
#define INT(x)   ((x) >> FP_SHIFT)

#define PADDLE_W       2
#define PADDLE_H       12
#define PADDLE_MARGIN  2
#define PADDLE_SPEED   (FP(3) / 2)
#define LEFT_PADDLE_X  PADDLE_MARGIN
#define RIGHT_PADDLE_X (SCREEN_W - PADDLE_MARGIN - PADDLE_W)

#define BALL_SIZE      3
#define BALL_SPEED     FP(2)
#define BALL_SPEED_MAX FP(4)

/* How far (in pixels) the AI may misjudge the ball position; > PADDLE_H / 2 allows misses. */
#define AI_AIM_ERROR 10

#define WIN_SCORE 9

#define POINT_PAUSE_MS 500
#define WIN_PAUSE_MS   3000

struct paddle {
	int32_t y;
	int32_t aim;
	uint8_t score;
};

struct ball {
	int32_t x, y;
	int32_t vx, vy;
};

static struct paddle left, right;
static struct ball ball;
static bool game_over;

static uint32_t rng_state;

static uint32_t rand32(void)
{
	/* xorshift32 */
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;

	return rng_state;
}

/* Random integer in [min, max]. */
static int32_t rand_range(int32_t min, int32_t max)
{
	return min + (int32_t)(rand32() % (uint32_t)(max - min + 1));
}

static void draw_frame(void)
{
	/* Scores on the left of the yellow area */
	gfx_draw_digit(2, 3, left.score, 2);
	gfx_fill_rect(12, 7, 5, 2);
	gfx_draw_digit(20, 3, right.score, 2);

	/* Field borders and dashed center net in the blue area */
	gfx_fill_rect(0, BORDER_TOP, SCREEN_W, 1);
	gfx_fill_rect(0, BORDER_BOT, SCREEN_W, 1);
	for (int y = FIELD_TOP + 1; y < FIELD_BOT; y += 4) {
		gfx_fill_rect(SCREEN_W / 2 - 1, y, 1, 2);
	}

	gfx_fill_rect(LEFT_PADDLE_X, INT(left.y), PADDLE_W, PADDLE_H);
	gfx_fill_rect(RIGHT_PADDLE_X, INT(right.y), PADDLE_W, PADDLE_H);
	gfx_fill_rect(INT(ball.x), INT(ball.y), BALL_SIZE, BALL_SIZE);
}

/* Pick a new aiming error so the AI occasionally misses. */
static void new_aim(struct paddle *p)
{
	p->aim = FP(rand_range(-AI_AIM_ERROR, AI_AIM_ERROR));
}

/* Serve the ball from the center toward the given direction (-1 left, 1 right). */
static void serve(int dir)
{
	ball.x = FP(SCREEN_W / 2 - BALL_SIZE / 2);
	ball.y = FP(rand_range(FIELD_TOP + 8, FIELD_BOT - 8 - BALL_SIZE));
	ball.vx = dir * BALL_SPEED;
	ball.vy = rand_range(-FP(3) / 2, FP(3) / 2);

	new_aim(&left);
	new_aim(&right);
}

static void reset_game(void)
{
	left = (struct paddle){.y = FP(FIELD_TOP + (FIELD_H - PADDLE_H) / 2)};
	right = left;
	serve(rand32() & 1 ? 1 : -1);
}

static void move_paddle(struct paddle *p, bool ball_incoming)
{
	int32_t target;

	/* Track the ball when it is incoming, otherwise drift back to the center. */
	if (ball_incoming) {
		target = ball.y + FP(BALL_SIZE) / 2 + p->aim - FP(PADDLE_H) / 2;
	} else {
		target = FP(FIELD_TOP + (FIELD_H - PADDLE_H) / 2);
	}

	p->y += CLAMP(target - p->y, -PADDLE_SPEED, PADDLE_SPEED);
	p->y = CLAMP(p->y, FP(FIELD_TOP), FP(FIELD_BOT - PADDLE_H));
}

/*
 * Bounce the ball off a paddle if they overlap. The bounce angle depends on
 * where the ball hits the paddle, and the ball speeds up slightly.
 */
static void paddle_collide(struct paddle *p, int paddle_x, int dir)
{
	int32_t ball_center, paddle_center;

	if (INT(ball.x) + BALL_SIZE <= paddle_x || INT(ball.x) >= paddle_x + PADDLE_W) {
		return;
	}
	if (ball.y + FP(BALL_SIZE) <= p->y || ball.y >= p->y + FP(PADDLE_H)) {
		return;
	}

	ball_center = ball.y + FP(BALL_SIZE) / 2;
	paddle_center = p->y + FP(PADDLE_H) / 2;

	ball.vx = dir * MIN(abs(ball.vx) + FP(1) / 8, BALL_SPEED_MAX);
	ball.vy = (ball_center - paddle_center) / 4;
	ball.x = FP(dir > 0 ? paddle_x + PADDLE_W : paddle_x - BALL_SIZE);

	new_aim(p);
}

/* Advance the game by one frame. Returns true when a point was scored. */
static bool update(void)
{
	move_paddle(&left, ball.vx < 0);
	move_paddle(&right, ball.vx > 0);

	ball.x += ball.vx;
	ball.y += ball.vy;

	/* Bounce off the field borders */
	if (ball.y < FP(FIELD_TOP)) {
		ball.y = 2 * FP(FIELD_TOP) - ball.y;
		ball.vy = -ball.vy;
	} else if (ball.y > FP(FIELD_BOT - BALL_SIZE)) {
		ball.y = 2 * FP(FIELD_BOT - BALL_SIZE) - ball.y;
		ball.vy = -ball.vy;
	}

	if (ball.vx < 0) {
		paddle_collide(&left, LEFT_PADDLE_X, 1);
	} else {
		paddle_collide(&right, RIGHT_PADDLE_X, -1);
	}

	if (INT(ball.x) + BALL_SIZE < 0) {
		right.score++;
		serve(-1);
		return true;
	}
	if (INT(ball.x) >= SCREEN_W) {
		left.score++;
		serve(1);
		return true;
	}

	return false;
}

static void pong_init(void)
{
	rng_state = k_cycle_get_32() | 1U;
	game_over = false;
	reset_game();
}

static int32_t pong_step(void)
{
	bool scored;

	/* A won game stays on screen for WIN_PAUSE_MS, then restarts */
	if (game_over) {
		reset_game();
		game_over = false;
	}

	scored = update();
	draw_frame();

	if (!scored) {
		return 0;
	}

	/* Short pause after each point, longer after a won game */
	if (left.score >= WIN_SCORE || right.score >= WIN_SCORE) {
		game_over = true;
		return WIN_PAUSE_MS;
	}

	return POINT_PAUSE_MS;
}

const struct animation pong_animation = {
	.name = "pong",
	.init = pong_init,
	.step = pong_step,
};
