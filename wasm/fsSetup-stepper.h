/* OpenCP Module Player - fsSetup Stepper for WASM
 * Copyright (c) 2025 - Non-blocking setup dialog
 *
 * This header provides the stepper infrastructure for fsSetup() dialog,
 * allowing it to run without blocking the browser.
 */

#ifndef WASM_FSSETUP_STEPPER_H
#define WASM_FSSETUP_STEPPER_H

/* Engine state for fsSetup stepper */
struct fsSetup_stepper_engine {
	int is_active;
	int first_run;
	int stored;  /* Config saved flag */
	int in_keyboard_help;  /* Currently showing keyboard help */
	int last_fps_current;  /* Last fsFPSCurrent value for update detection */
};

/* Initialize the stepper engine */
void fsSetup_stepper_init(struct fsSetup_stepper_engine *engine);

/* Run one iteration of the setup dialog (returns 1 when done, 0 if still running) */
int fsSetup_stepper_run(struct fsSetup_stepper_engine *engine);

/* Check if stepper is active */
int fsSetup_stepper_is_active(const struct fsSetup_stepper_engine *engine);

/* Finish and cleanup */
void fsSetup_stepper_finish(struct fsSetup_stepper_engine *engine);

/* Get global engine instance */
struct fsSetup_stepper_engine *fsSetup_stepper_get_engine(void);

#endif /* WASM_FSSETUP_STEPPER_H */
