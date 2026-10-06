/* Non-blocking file-selector help for the browser build.
 * The desktop fsHelp2() waits on the keyboard inside framelock(). That never
 * returns to the browser, so the next key cannot arrive. The file selector
 * drives this stepper one frame at a time instead.
 */

#ifndef WASM_FSHELP_STEPPER_H
#define WASM_FSHELP_STEPPER_H

struct fsHelp_stepper_engine {
	int is_active;
};

void fsHelp_stepper_init(struct fsHelp_stepper_engine *engine);
int fsHelp_stepper_run(struct fsHelp_stepper_engine *engine);
int fsHelp_stepper_is_active(const struct fsHelp_stepper_engine *engine);
void fsHelp_stepper_finish(struct fsHelp_stepper_engine *engine);
struct fsHelp_stepper_engine *fsHelp_stepper_get_engine(void);

/* Starts one help session and returns. pfilesel.c calls this in place of fsHelp2(). */
unsigned char wasm_fsHelp2_arm(void);

#endif
