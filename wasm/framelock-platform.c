/* OpenCP Module Player - WASM Platform Framelock Implementation
 * Copyright (c) 2025 - WASM port with configurable timing
 *
 * This file provides WASM-specific implementations for framelock timing,
 * allowing the browser to drive rendering without sleep/gettimeofday.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include <emscripten.h>
#include "../stuff/framelock.h"
#include "../stuff/poll.h"

/* WASM uses Emscripten's main loop instead of sleep-based timing */

/* Platform-specific state */
static int wasm_no_sleep_mode = 1;  /* Always enabled for browser */
static double last_frame_time = 0.0;
static int frame_count = 0;

/* Platform initialization hook called by framelock_init() */
void framelock_platform_init(void)
{
	/* In WASM, FPS is driven by emscripten_set_main_loop
	 * We honor fsFPS for compatibility but don't sleep */
	wasm_no_sleep_mode = 1;
	last_frame_time = emscripten_get_now();
	frame_count = 0;
}

/* Platform-specific sleep implementation (no-op for WASM) */
void framelock_platform_sleep(unsigned long usec)
{
	/* WASM doesn't sleep - the browser controls frame timing */
	(void)usec;
}

/* Platform-specific timer query */
unsigned long framelock_platform_get_time_usec(void)
{
	/* Return current time in microseconds */
	return (unsigned long)(emscripten_get_now() * 1000.0);
}

/* Check if platform requires sleep (false for WASM) */
int framelock_platform_needs_sleep(void)
{
	return 0;  /* Browser drives timing, no sleep needed */
}

/* WASM-specific: Check if a frame interval has passed */
int framelock_platform_should_render(int target_fps)
{
	double current_time = emscripten_get_now();
	double frame_interval = 1000.0 / (double)target_fps;  /* ms per frame */

	if ((current_time - last_frame_time) >= frame_interval) {
		last_frame_time = current_time;
		frame_count++;

		/* framelock() invokes tmTimerHandler() after this returns true. */
		return 1;
	}

	return 0;
}

/* WASM-specific: Force immediate render (for poll_framelock) */
void framelock_platform_force_render(void)
{
	tmTimerHandler(pollTypeVideo);
	last_frame_time = emscripten_get_now();
	frame_count++;
}

/* Get current frame count */
int framelock_platform_get_frame_count(void)
{
	return frame_count;
}

/* Allow browser to configure "no sleep" mode (always true for WASM) */
void framelock_platform_set_no_sleep_mode(int enabled)
{
	wasm_no_sleep_mode = enabled;
}

int framelock_platform_get_no_sleep_mode(void)
{
	return wasm_no_sleep_mode;
}
