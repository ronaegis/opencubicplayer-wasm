/* OpenCP Module Player - Platform Framelock API
 * Copyright (c) 2025 - Cross-platform timing abstraction
 *
 * This header defines platform-specific hooks for framelock timing.
 * Each platform (Unix, Windows, WASM) provides its own implementation.
 */

#ifndef FRAMELOCK_PLATFORM_H
#define FRAMELOCK_PLATFORM_H

/* Platform initialization - called by framelock_init() */
void framelock_platform_init(void);

/* Platform sleep - called when framelock needs to wait */
void framelock_platform_sleep(unsigned long usec);

/* Platform time query - returns current time in microseconds */
unsigned long framelock_platform_get_time_usec(void);

/* Check if this platform needs sleep-based timing (false for WASM) */
int framelock_platform_needs_sleep(void);

/* WASM-specific: Check if frame interval has passed */
int framelock_platform_should_render(int target_fps);

/* WASM-specific: Force immediate render (for poll_framelock) */
void framelock_platform_force_render(void);

/* Get current frame count */
int framelock_platform_get_frame_count(void);

/* Configure "no sleep" mode (browser-driven vs traditional timing) */
void framelock_platform_set_no_sleep_mode(int enabled);
int framelock_platform_get_no_sleep_mode(void);

#endif /* FRAMELOCK_PLATFORM_H */
