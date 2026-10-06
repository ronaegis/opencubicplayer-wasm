/* OpenCP Module Player - WASM Loading Progress Reporter
 * Copyright (c) 2025 - Reports plugin loading status to JavaScript
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <emscripten.h>

/* Report loading progress to JavaScript UI */
void wasm_report_loading_status(const char *message)
{
	if (!message) {
		return;
	}

	/* Always log to console */
	fprintf(stderr, "[OCP Loading] %s\n", message);

	/* Call JavaScript updateLoadingStatus function */
	EM_ASM({
		if (typeof Module !== 'undefined' && Module.updateLoadingStatus) {
			Module.updateLoadingStatus(UTF8ToString($0));
		} else if (typeof window !== 'undefined' && window.updateLoadingStatus) {
			window.updateLoadingStatus(UTF8ToString($0));
		} else {
			console.log('[OCP Loading] (no JS handler) ' + UTF8ToString($0));
		}
	}, message);
}
