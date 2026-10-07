/* OpenCP Module Player - WASM Platform Configuration JavaScript API
 * Copyright (c) 2025-2026 Christophe Thibault - JavaScript-accessible configuration API
 *
 * This file exports functions that allow JavaScript to configure
 * WASM platform behavior at runtime (paths, timing, etc.).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include <stdio.h>
#include <emscripten.h>
#include "psetting-platform.h"
#include "framelock-platform.h"

/* Set a virtual filesystem path from JavaScript
 *
 * Usage from JavaScript:
 *   Module.ccall('wasm_set_virtual_path', 'number',
 *                ['string', 'string'], ['program', '/custom/program/']);
 *
 * Supported keys: program, autoload, home, config, datahome, data, temp
 */
EMSCRIPTEN_KEEPALIVE
int wasm_set_virtual_path(const char *key, const char *value)
{
	if (!key || !value) {
		fprintf(stderr, "WASM: wasm_set_virtual_path: invalid arguments\n");
		return -1;
	}

	return psetting_platform_set_virtual_path(key, value);
}

/* Get a virtual filesystem path from JavaScript
 *
 * Usage from JavaScript:
 *   const path = Module.ccall('wasm_get_virtual_path', 'string',
 *                             ['string'], ['program']);
 *
 * Supported keys: program, autoload, home, config, datahome, data, temp
 */
EMSCRIPTEN_KEEPALIVE
const char *wasm_get_virtual_path(const char *key)
{
	if (!key) {
		fprintf(stderr, "WASM: wasm_get_virtual_path: invalid arguments\n");
		return NULL;
	}

	return psetting_platform_get_virtual_path(key);
}

/* Configure "no sleep" mode for framelock
 *
 * Usage from JavaScript:
 *   Module.ccall('wasm_set_no_sleep_mode', 'void',
 *                ['number'], [1]);  // 1 = enabled, 0 = disabled
 *
 * In WASM, this is always enabled (browser drives timing).
 */
EMSCRIPTEN_KEEPALIVE
void wasm_set_no_sleep_mode(int enabled)
{
	framelock_platform_set_no_sleep_mode(enabled);
}

/* Get current "no sleep" mode status
 *
 * Usage from JavaScript:
 *   const noSleep = Module.ccall('wasm_get_no_sleep_mode', 'number', [], []);
 */
EMSCRIPTEN_KEEPALIVE
int wasm_get_no_sleep_mode(void)
{
	return framelock_platform_get_no_sleep_mode();
}

/* Get current frame count (for debugging)
 *
 * Usage from JavaScript:
 *   const frames = Module.ccall('wasm_get_frame_count', 'number', [], []);
 */
EMSCRIPTEN_KEEPALIVE
int wasm_get_frame_count(void)
{
	return framelock_platform_get_frame_count();
}

/* Force a frame render (for manual timing control)
 *
 * Usage from JavaScript:
 *   Module.ccall('wasm_force_frame_render', 'void', [], []);
 */
EMSCRIPTEN_KEEPALIVE
void wasm_force_frame_render(void)
{
	framelock_platform_force_render();
}

/* Get all configured paths as JSON (for debugging)
 *
 * Usage from JavaScript:
 *   const pathsJson = Module.ccall('wasm_get_paths_json', 'string', [], []);
 *   console.log(pathsJson);
 */
EMSCRIPTEN_KEEPALIVE
const char *wasm_get_paths_json(void)
{
	static char json_buffer[2048];

	snprintf(json_buffer, sizeof(json_buffer),
	         "{\"program\":\"%s\",\"autoload\":\"%s\",\"home\":\"%s\","
	         "\"config\":\"%s\",\"datahome\":\"%s\",\"data\":\"%s\",\"temp\":\"%s\"}",
	         psetting_platform_get_virtual_path("program") ?: "",
	         psetting_platform_get_virtual_path("autoload") ?: "",
	         psetting_platform_get_virtual_path("home") ?: "",
	         psetting_platform_get_virtual_path("config") ?: "",
	         psetting_platform_get_virtual_path("datahome") ?: "",
	         psetting_platform_get_virtual_path("data") ?: "",
	         psetting_platform_get_virtual_path("temp") ?: "");

	return json_buffer;
}
