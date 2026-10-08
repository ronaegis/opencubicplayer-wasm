/* OpenCP Module Player - WASM Config Wrapper
 * Copyright (c) 2025-2026 Christophe Thibault - WASM port with IDBFS persistence
 *
 * This file wraps the config save operation to trigger IDBFS sync
 * after configuration changes are written to disk.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include <stdio.h>
#include "../boot/psetting.h"
#include "idbfs-persistence.h"

/* Rename the original StoreConfig from configAPI so we can wrap it */
#define ORIGINAL_STORECONFIG_IMPL
extern struct configAPI_t configAPI;

/* Store the original StoreConfig function pointer */
static int (*original_StoreConfig)(void) = NULL;

/* WASM wrapper for StoreConfig that triggers IDBFS sync */
static int wasm_StoreConfig(void)
{
	int result;

	/* Call the original StoreConfig */
	if (!original_StoreConfig) {
		fprintf(stderr, "WASM: original_StoreConfig is NULL!\n");
		return -1;
	}

	result = original_StoreConfig();

	if (result == 0) {
		/* Config saved successfully - trigger IDBFS sync */
#ifdef OCP_WASM_DEBUG_LOGGING
		fprintf(stderr, "WASM: Config saved, marking for IDBFS sync\n");
#endif
		idbfs_mark_dirty_config();
	}

	return result;
}

/* Initialize the WASM config wrapper */
void wasm_config_wrapper_init(void)
{
	/* Store the original StoreConfig pointer */
	original_StoreConfig = configAPI.StoreConfig;

	if (!original_StoreConfig) {
		fprintf(stderr, "WASM: Warning - configAPI.StoreConfig is NULL, wrapper not installed\n");
		return;
	}

	/* Replace with our wrapper */
	configAPI.StoreConfig = wasm_StoreConfig;

	fprintf(stderr, "WASM: Config wrapper installed (IDBFS persistence enabled)\n");
}
