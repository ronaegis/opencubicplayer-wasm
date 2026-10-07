/* OpenCP Module Player - WASM IDBFS Persistence
 * Copyright (c) 2025-2026 Christophe Thibault - WASM port with IndexedDB persistence
 *
 * This file provides IndexedDB-backed filesystem (IDBFS) persistence
 * for configuration and cached data across browser sessions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef WASM_IDBFS_PERSISTENCE_H
#define WASM_IDBFS_PERSISTENCE_H

/**
 * Initialize IDBFS mounts for persistent storage.
 * Must be called early in initialization, before any config or data access.
 *
 * Mounts:
 * - ConfigHomePath: For ocp.ini config file
 * - DataHomePath: For modland cache and database
 *
 * @return 0 on success, -1 on error
 */
int idbfs_init(void);

/**
 * Synchronize IDBFS to/from IndexedDB (async operation).
 * This starts an async sync and returns immediately.
 * The sync completion is tracked internally.
 *
 * @param populate If 1, load from IndexedDB. If 0, save to IndexedDB.
 * @return 0 if sync started successfully, -1 on error
 */
int idbfs_sync_async(int populate);

/**
 * Check if an async IDBFS sync is currently in progress.
 *
 * @return 1 if sync in progress, 0 if idle
 */
int idbfs_sync_in_progress(void);

/**
 * Sync config directory to IndexedDB (async).
 * Called after config changes (e.g., after SetProfileString/StoreConfig).
 * This is debounced to avoid excessive syncs.
 *
 * @return 0 on success, -1 on error
 */
int idbfs_sync_config(void);

/**
 * Sync data directory to IndexedDB (async).
 * Called after modland database updates or file downloads.
 * This is debounced to avoid excessive syncs.
 *
 * @return 0 on success, -1 on error
 */
int idbfs_sync_data(void);

/**
 * Sync data directory to IndexedDB immediately (async).
 * Bypasses debounce timer - use for important operations like modland save completion.
 *
 * @return 0 on success, -1 on error
 */
int idbfs_sync_data_immediate(void);

/**
 * Mark that sync is needed (deferred).
 * Used for batching/debouncing multiple rapid changes.
 */
void idbfs_mark_dirty_config(void);
void idbfs_mark_dirty_data(void);

/**
 * Periodic maintenance - called from main loop.
 * Triggers deferred syncs if directories are marked dirty.
 */
void idbfs_tick(void);

typedef void (*idbfs_sync_callback_t)(int success, void *user_data);

/**
 * Register a callback that fires when the current IDBFS sync (if any)
 * completes. The callback executes on the main thread.
 *
 * If no sync is active when this function is called, the callback runs
 * immediately before the function returns.
 *
 * Only one waiter may be registered at a time; attempting to queue a second
 * waiter while one is active returns -1.
 *
 * @param timeout_ms Timeout in milliseconds. Pass a negative value to wait
 *                   indefinitely. When the timeout elapses before the sync
 *                   completes, the callback fires with success == 0.
 * @param callback   Callback invoked when the sync finishes or times out.
 * @param user_data  Opaque pointer forwarded to the callback.
 * @return 0 if the wait was scheduled, -1 if a waiter is already active.
 */
int idbfs_wait_for_sync(int timeout_ms, idbfs_sync_callback_t callback, void *user_data);

/**
 * Cleanup IDBFS resources.
 * Should be called during shutdown.
 */
void idbfs_close(void);

#endif /* WASM_IDBFS_PERSISTENCE_H */
