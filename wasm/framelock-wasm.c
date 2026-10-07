/* OpenCP Module Player - WASM Framelock Implementation
 * Copyright (c) 2025-2026 Christophe Thibault - WASM port using platform callbacks
 *
 * This file provides framelock implementation for WASM that:
 * - Uses browser-driven timing instead of sleep/gettimeofday
 * - Allows configuration through platform API
 * - Maintains compatibility with OCP's timing expectations
 *
 * Based on stuff/framelock.c but adapted for WASM constraints.
 */

#include "config.h"
#include "../types.h"
#include "../boot/psetting.h"
#include "../stuff/framelock.h"
#include "../stuff/poll.h"
#include "framelock-platform.h"

/* Public variables from original framelock.c */
int fsFPS = 25;
int fsFPSCurrent = 0;

/* Internal state */
static int Current = 0;
static int PendingPoll = 0;
static unsigned long target_frame_usec = 0;
static unsigned long target_audio_usec = 0;
static unsigned long last_second = 0;

OCP_INTERNAL void framelock_init(void)
{
	/* Read FPS from config like the original */
	fsFPS = cfGetProfileInt("screen", "fps", 20, 0);
	if (fsFPS <= 0)
		fsFPS = 20;

	/* Initialize platform-specific timing */
	framelock_platform_init();

	/* Calculate frame intervals */
	target_frame_usec = 1000000 / fsFPS;
	target_audio_usec = 1000000 / 50;  /* Audio poll at 50Hz */
	last_second = framelock_platform_get_time_usec() / 1000000;
	Current = 0;
	fsFPSCurrent = 0;
	PendingPoll = 0;
}

/* Audio poll helper (from original framelock.c) */
static void AudioPoll(unsigned long curr_usec)
{
	unsigned long curr_sec = curr_usec / 1000000;
	unsigned long usec_in_sec = curr_usec % 1000000;

	if (curr_sec != last_second) {
		last_second = curr_sec;
		target_audio_usec = 1000000 / 50;
		tmTimerHandler(pollTypeAudio);
	} else if (usec_in_sec >= target_audio_usec) {
		target_audio_usec += 1000000 / 50;
		tmTimerHandler(pollTypeAudio);
	}
}

void framelock(void)
{
	unsigned long curr_usec;
	unsigned long curr_sec;
	unsigned long usec_in_sec;

	PendingPoll = 0;

	/* Get current time from platform */
	curr_usec = framelock_platform_get_time_usec();
	curr_sec = curr_usec / 1000000;
	usec_in_sec = curr_usec % 1000000;

	/* Handle audio polling for low FPS */
	if (fsFPS < 50) {
		AudioPoll(curr_usec);
	}

	/* Check if we've crossed into a new second */
	if (curr_sec != last_second) {
		fsFPSCurrent = Current;
		Current = 1;
		last_second = curr_sec;
		target_frame_usec = 1000000 / fsFPS;

		/* Same split as stuff/framelock.c: audio at the frame rate when it
		 * is already at least 50Hz, and a video refresh every new second. */
		if (fsFPS >= 50) {
			tmTimerHandler(pollTypeAudio);
		}
		tmTimerHandler(pollTypeVideo);
		return;
	}

	/* Check if we need to wait for next frame */
	if (framelock_platform_needs_sleep()) {
		/* Traditional platforms: sleep until next frame */
		if (usec_in_sec < target_frame_usec) {
			framelock_platform_sleep(target_frame_usec - usec_in_sec);
		}
	} else {
		/* WASM: browser controls timing, no sleep */
		/* Just check if we should render this frame */
		if (!framelock_platform_should_render(fsFPS)) {
			return;  /* Not time for next frame yet */
		}
	}

	target_frame_usec += 1000000 / fsFPS;

	tmTimerHandler(pollTypeVideo);
	if (fsFPS >= 50) {
		tmTimerHandler(pollTypeAudio);
	}

	Current++;
}

void preemptive_framelock(void)
{
	unsigned long curr_usec = framelock_platform_get_time_usec();
	unsigned long curr_sec = curr_usec / 1000000;
	unsigned long usec_in_sec = curr_usec % 1000000;

	/* Handle audio polling for low FPS */
	if (fsFPS < 50) {
		AudioPoll(curr_usec);
	}

	/* Check for new second */
	if (curr_sec != last_second) {
		fsFPSCurrent = Current;
		Current = 1;
		last_second = curr_sec;
		target_frame_usec = 1000000 / fsFPS;
		PendingPoll = 1;
		return;
	}

	/* Check if we're early */
	if (usec_in_sec < target_frame_usec) {
		return;  /* Too early, don't update yet */
	}

	target_frame_usec += 1000000 / fsFPS;

	/* preemptive_framelock() may push audio, not paint. */
	if (fsFPS >= 50) {
		tmTimerHandler(pollTypeAudio);
	}

	Current++;
	PendingPoll = 1;
}

int poll_framelock(void)
{
	unsigned long curr_usec = framelock_platform_get_time_usec();
	unsigned long curr_sec = curr_usec / 1000000;
	unsigned long usec_in_sec = curr_usec % 1000000;

	/* Handle audio polling for low FPS */
	if (fsFPS < 50) {
		AudioPoll(curr_usec);
	}

	/* Check for new second */
	if (curr_sec != last_second) {
		fsFPSCurrent = Current;
		Current = 1;
		last_second = curr_sec;
		target_frame_usec = 1000000 / fsFPS;
		tmTimerHandler(pollTypeVideo);
		if (fsFPS >= 50) {
			tmTimerHandler(pollTypeAudio);
		}
		PendingPoll = 0;
		return 1;
	}

	/* Check if we're early */
	if (usec_in_sec < target_frame_usec) {
		/* Return pending poll if we have one */
		if (PendingPoll) {
			PendingPoll = 0;
			return 1;
		}
		return 0;  /* Too early */
	}

	target_frame_usec += 1000000 / fsFPS;

	tmTimerHandler(pollTypeVideo);
	if (fsFPS >= 50) {
		tmTimerHandler(pollTypeAudio);
	}

	Current++;
	PendingPoll = 0;
	return 1;
}
