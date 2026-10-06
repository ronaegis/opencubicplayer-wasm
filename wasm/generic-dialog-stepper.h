/* generic-dialog-stepper.h - Generic dialog stepper for WASM non-blocking event loops
 *
 * PURPOSE:
 *   Provides a reusable framework for converting blocking while(1) config dialogs
 *   into non-blocking steppers that yield control back to the browser event loop.
 *
 * APPROACH:
 *   - Dialog logic is split into callbacks (init, draw, handle_key, cleanup)
 *   - Generic engine manages the event loop iteration
 *   - Dialog-specific state is encapsulated in a custom structure
 *   - Same code works in both WASM (stepped) and native (blocking) builds
 *
 * USAGE:
 *   1. Define dialog state structure (all dialog variables)
 *   2. Implement callback functions (init, draw, handle_key, cleanup)
 *   3. Create dialog_ops_t structure with callbacks
 *   4. Call generic_dialog_run() with iteration_limit=1 (WASM) or -1 (native)
 *
 * Last updated: 2025-10-24
 */

#ifndef _WASM_GENERIC_DIALOG_STEPPER_H
#define _WASM_GENERIC_DIALOG_STEPPER_H

#include "filesel/filesystem-file-dev.h"

/* Dialog operation callbacks - implement these for each dialog
 *
 * All callbacks receive 'state' which is the dialog-specific state structure.
 * Callbacks can modify state freely - it persists across frames.
 */
struct dialog_ops_t {
	const char *name; /* For debugging/logging */

	/* Initialize dialog state (called once on entry)
	 * Use this to set initial values for selection indices, etc.
	 */
	void (*init)(void *state, void **token, const struct DevInterfaceAPI_t *API);

	/* Draw the dialog (called every frame)
	 * Should use API->fsDraw() has already been called by the framework.
	 */
	void (*draw)(void *state, const struct DevInterfaceAPI_t *API);

	/* Handle one keyboard event
	 * Return: 0 = continue dialog, 1 = exit dialog
	 * This is called in a loop while KeyboardHit() returns true.
	 */
	int (*handle_key)(void *state, int key, const struct DevInterfaceAPI_t *API);

	/* Cleanup dialog state (called on exit)
	 * Use this to save config, free resources, etc.
	 */
	void (*cleanup)(void *state, void **token, const struct DevInterfaceAPI_t *API);
};

/* Generic dialog engine - manages the event loop
 * This structure should be static/global to persist across Run() calls in WASM.
 */
struct generic_dialog_engine {
	const struct dialog_ops_t *ops;
	void *state;                    /* Dialog-specific state */
	size_t state_size;              /* Size of state structure */
	int is_active;
	int should_exit;
	int first_frame;
};

/* Initialize the engine with dialog-specific operations and state
 * Must be called before first generic_dialog_run().
 *
 * engine: Pre-allocated engine structure (usually static)
 * ops: Pointer to dialog_ops_t with callbacks
 * state: Pointer to dialog state structure (usually static)
 * state_size: sizeof(dialog state structure)
 */
void generic_dialog_init(
	struct generic_dialog_engine *engine,
	const struct dialog_ops_t *ops,
	void *state,
	size_t state_size
);

/* Run the dialog for up to iteration_limit frames
 *
 * Returns: 0 = dialog exited normally, 1 = still running (call again)
 *
 * iteration_limit:
 *   1 = single-step mode (WASM - yields after one frame)
 *  -1 = unlimited iterations (native blocking mode)
 *   N > 1 = run N frames then yield
 *
 * token: Passed through to callbacks
 * API: DevInterfaceAPI with console, config, etc.
 */
int generic_dialog_run(
	struct generic_dialog_engine *engine,
	void **token,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit
);

/* Check if the dialog engine is currently active
 * Returns: 1 if active, 0 if not
 */
int generic_dialog_is_active(const struct generic_dialog_engine *engine);

/* Reset the dialog engine to initial state
 * Called automatically on exit, but can be called manually if needed.
 */
void generic_dialog_reset(struct generic_dialog_engine *engine);

#endif
