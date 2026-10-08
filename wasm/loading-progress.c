/* OpenCP Module Player - WASM Loading Progress Reporter
 * Copyright (c) 2025-2026 Christophe Thibault - Reports plugin loading status to JavaScript
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

static void wasm_call_page(const char *fn, const char *message)
{
	if (!fn || !message) {
		return;
	}

	EM_ASM({
		var name = UTF8ToString($0);
		var text = UTF8ToString($1);
		var pageFn = (typeof window !== 'undefined') ? window[name] : null;
		if (typeof pageFn === 'function') {
			pageFn(text);
		} else {
			console.error('[OCP] ' + name + ' is missing: ' + text);
		}
	}, fn, message);
}

void wasm_report_page_error(const char *message)
{
	if (!message) {
		return;
	}
	fprintf(stderr, "[OCP] %s\n", message);
	wasm_call_page("ocpShowFatalError", message);
}

void wasm_report_page_notice(const char *message)
{
	if (!message) {
		return;
	}
	fprintf(stderr, "[OCP] %s\n", message);
	wasm_call_page("ocpShowNotice", message);
}
