/* OpenCP Module Player - WASM IDBFS Persistence Implementation
 * Copyright (c) 2025-2026 Christophe Thibault - WASM port with IndexedDB persistence
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include "idbfs-persistence.h"

/* Sync state tracking */
static int idbfs_initialized = 0;
static int idbfs_sync_active = 0;
static int idbfs_config_dirty = 0;
static int idbfs_data_dirty = 0;
static time_t idbfs_last_config_sync = 0;
static time_t idbfs_last_data_sync = 0;

static idbfs_sync_callback_t idbfs_wait_callback = NULL;
static void *idbfs_wait_userdata = NULL;
static double idbfs_wait_deadline = 0.0;
static int idbfs_wait_timeout_ms = -1;
static int idbfs_wait_keepalive = 0;
static int idbfs_wait_active = 0;

/* Debounce settings - don't sync more often than this */
#define IDBFS_SYNC_DEBOUNCE_SECONDS 5

/* JavaScript callbacks for IDBFS operations */
EM_JS(int, js_idbfs_mount_and_sync_populate, (), {
	try {
		// Mount config/data directory at .ocp
		// Parent directories (/home, /home/web_user) must already exist
		FS.mkdir('/home/web_user/.ocp');
		FS.mount(IDBFS, {}, '/home/web_user/.ocp');

		// Populate from IndexedDB (load saved data)
		FS.syncfs(true, function(err) {
			if (err) {
				console.error('IDBFS populate error:', err);
				Module._idbfs_sync_complete_callback(0);
			} else {
				console.log('IDBFS: Successfully populated from IndexedDB');
				Module._idbfs_sync_complete_callback(1);
			}
		});

		return 0;
	} catch (e) {
		console.error('IDBFS mount error:', e);
		// If directory already exists (from re-init), unmount and retry
		try {
			FS.unmount('/home/web_user/.ocp');
		} catch (e2) {}
		return -1;
	}
});

EM_JS(int, js_idbfs_sync_to_storage, (), {
	try {
		// Persist to IndexedDB (save)
		FS.syncfs(false, function(err) {
			if (err) {
				console.error('IDBFS persist error:', err);
				Module._idbfs_sync_complete_callback(0);
			} else {
				console.log('IDBFS: Successfully persisted to IndexedDB');
				Module._idbfs_sync_complete_callback(1);
			}
		});
		return 0;
	} catch (e) {
		console.error('IDBFS sync error:', e);
		return -1;
	}
});

static void idbfs_dispatch_wait_callback(int success)
{
	if (!idbfs_wait_active) {
		return;
	}

	idbfs_sync_callback_t callback = idbfs_wait_callback;
	void *userdata = idbfs_wait_userdata;

	idbfs_wait_callback = NULL;
	idbfs_wait_userdata = NULL;
	idbfs_wait_active = 0;
	idbfs_wait_timeout_ms = -1;
	idbfs_wait_deadline = 0.0;

	if (idbfs_wait_keepalive) {
		emscripten_runtime_keepalive_pop();
		idbfs_wait_keepalive = 0;
	}

	if (callback) {
		callback(success, userdata);
	}
}

static void idbfs_wait_poll(void *arg)
{
	(void)arg;

	if (!idbfs_wait_active) {
		return;
	}

	if (!idbfs_sync_active) {
		idbfs_dispatch_wait_callback(1);
		return;
	}

	if (idbfs_wait_timeout_ms >= 0) {
		double now = emscripten_get_now();
		if (now >= idbfs_wait_deadline) {
			fprintf(stderr, "IDBFS: Wait timed out after %d ms\n", idbfs_wait_timeout_ms);
			idbfs_dispatch_wait_callback(0);
			return;
		}
	}

	emscripten_async_call(idbfs_wait_poll, NULL, 10);
}

/* Callback from JavaScript when sync completes */
EMSCRIPTEN_KEEPALIVE
void idbfs_sync_complete_callback(int success)
{
	idbfs_sync_active = 0;

	if (success) {
		fprintf(stderr, "IDBFS: Sync completed successfully\n");
	} else {
		fprintf(stderr, "IDBFS: Sync failed\n");
	}

	if (idbfs_wait_active) {
		idbfs_dispatch_wait_callback(success);
	}
}

int idbfs_init(void)
{
	if (idbfs_initialized) {
		return 0;
	}

	fprintf(stderr, "IDBFS: Initializing IndexedDB persistence...\n");

	/* Set up initial state */
	idbfs_sync_active = 1; /* Will be cleared by callback */

	/* Mount IDBFS and populate from storage */
	if (js_idbfs_mount_and_sync_populate() < 0) {
		fprintf(stderr, "IDBFS: Failed to mount and populate\n");
		idbfs_sync_active = 0;
		return -1;
	}

	idbfs_initialized = 1;
	fprintf(stderr, "IDBFS: Initialization started (async populate in progress)\n");

	return 0;
}

int idbfs_sync_async(int populate)
{
	if (!idbfs_initialized) {
		fprintf(stderr, "IDBFS: Not initialized, cannot sync\n");
		return -1;
	}

	if (idbfs_sync_active) {
		fprintf(stderr, "IDBFS: Sync already in progress, skipping\n");
		return 0;
	}

	idbfs_sync_active = 1;

	if (populate) {
		/* Load from IndexedDB - use the mount function which includes populate */
		if (js_idbfs_mount_and_sync_populate() < 0) {
			idbfs_sync_active = 0;
			return -1;
		}
	} else {
		/* Save to IndexedDB */
		if (js_idbfs_sync_to_storage() < 0) {
			idbfs_sync_active = 0;
			return -1;
		}
	}

	return 0;
}

int idbfs_sync_in_progress(void)
{
	return idbfs_sync_active;
}

int idbfs_sync_config(void)
{
	time_t now = time(NULL);

	/* Debounce: don't sync if we synced recently */
	if (now - idbfs_last_config_sync < IDBFS_SYNC_DEBOUNCE_SECONDS) {
		idbfs_config_dirty = 1;
		return 0;
	}

	fprintf(stderr, "IDBFS: Syncing config directory to IndexedDB...\n");

	if (idbfs_sync_async(0) < 0) {
		return -1;
	}

	idbfs_last_config_sync = now;
	idbfs_config_dirty = 0;

	return 0;
}

int idbfs_sync_data(void)
{
	time_t now = time(NULL);

	/* Debounce: don't sync if we synced recently */
	if (now - idbfs_last_data_sync < IDBFS_SYNC_DEBOUNCE_SECONDS) {
		idbfs_data_dirty = 1;
		return 0;
	}

	fprintf(stderr, "IDBFS: Syncing data directory to IndexedDB...\n");

	if (idbfs_sync_async(0) < 0) {
		return -1;
	}

	idbfs_last_data_sync = now;
	idbfs_data_dirty = 0;

	return 0;
}

int idbfs_sync_data_immediate(void)
{
	time_t now = time(NULL);

	fprintf(stderr, "IDBFS: Syncing data directory to IndexedDB (immediate)...\n");

	if (idbfs_sync_async(0) < 0) {
		return -1;
	}

	idbfs_last_data_sync = now;
	idbfs_data_dirty = 0;

	return 0;
}

void idbfs_mark_dirty_config(void)
{
	idbfs_config_dirty = 1;
}

void idbfs_mark_dirty_data(void)
{
	idbfs_data_dirty = 1;
}

void idbfs_tick(void)
{
	time_t now = time(NULL);

	/* Don't tick if sync is already in progress */
	if (idbfs_sync_active) {
		return;
	}

	/* Check for deferred config sync */
	if (idbfs_config_dirty && (now - idbfs_last_config_sync >= IDBFS_SYNC_DEBOUNCE_SECONDS)) {
		idbfs_sync_config();
	}

	/* Check for deferred data sync */
	if (idbfs_data_dirty && (now - idbfs_last_data_sync >= IDBFS_SYNC_DEBOUNCE_SECONDS)) {
		idbfs_sync_data();
	}
}

int idbfs_wait_for_sync(int timeout_ms, idbfs_sync_callback_t callback, void *user_data)
{
	if (!callback) {
		return -1;
	}

	if (!idbfs_sync_active) {
		callback(1, user_data);
		return 0;
	}

	if (idbfs_wait_active) {
		return -1;
	}

	emscripten_runtime_keepalive_push();
	idbfs_wait_keepalive = 1;

	idbfs_wait_active = 1;
	idbfs_wait_callback = callback;
	idbfs_wait_userdata = user_data;
	idbfs_wait_timeout_ms = timeout_ms;
	idbfs_wait_deadline = (timeout_ms >= 0) ? (emscripten_get_now() + timeout_ms) : 0.0;

	if (!idbfs_sync_active) {
		idbfs_dispatch_wait_callback(1);
		return 0;
	}

	emscripten_async_call(idbfs_wait_poll, NULL, 10);
	return 0;
}

static void idbfs_shutdown_waiter(int success, void *user_data)
{
	(void)user_data;

	if (!success) {
		fprintf(stderr, "IDBFS: Final sync did not complete before shutdown\n");
	}
}

void idbfs_close(void)
{
	if (!idbfs_initialized) {
		return;
	}

	fprintf(stderr, "IDBFS: Shutting down, performing final sync...\n");

	/* Force final sync */
	if (!idbfs_sync_active) {
		idbfs_sync_async(0);
	}

	if (idbfs_sync_active) {
		if (idbfs_wait_for_sync(5000, idbfs_shutdown_waiter, NULL) < 0) {
			fprintf(stderr, "IDBFS: Unable to wait for shutdown sync (wait already active)\n");
		}
	}

	idbfs_initialized = 0;
}
