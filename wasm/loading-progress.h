/* OpenCP Module Player - WASM Loading Progress Reporter
 * Copyright (c) 2025-2026 Christophe Thibault - Reports plugin loading status to JavaScript
 */

#ifndef WASM_LOADING_PROGRESS_H
#define WASM_LOADING_PROGRESS_H

/* Report loading progress to JavaScript UI */
void wasm_report_loading_status(const char *message);

/* Persistent overlay. Does not auto-hide. */
void wasm_report_page_error(const char *message);

/* Non-fatal notice. Startup continues. */
void wasm_report_page_notice(const char *message);

#endif /* WASM_LOADING_PROGRESS_H */
