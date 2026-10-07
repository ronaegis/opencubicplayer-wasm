/* WASM file selector wrapper.
 * We include the original pfilesel.c with fsFileSelect renamed so we can
 * provide a stepper-friendly implementation without touching upstream sources.
 */

#include "config.h"
#include <emscripten.h>
#include "fsSetup-stepper.h"
#include "fsHelp-stepper.h"

#define fsFileSelect fsFileSelect_blocking
#define fsInit fsInit_blocking
#define VirtualInterfaceRun VirtualInterfaceRun_blocking
#define fsHelp2 wasm_fsHelp2_arm
#include "../filesel/pfilesel.c"
#undef fsFileSelect
#undef fsInit
#undef VirtualInterfaceRun
#undef fsHelp2

#ifdef OCP_WASM_FILESEL_STEPPER
static struct fs_file_select_engine fs_stepper_engine;

int fsFileSelectIsActive(void)
{
	struct fsSetup_stepper_engine *setup_engine = fsSetup_stepper_get_engine();
	struct fsHelp_stepper_engine *help_engine = fsHelp_stepper_get_engine();

	/* File selector is active if the main stepper, setup stepper, or help stepper is running */
	return fsFileSelectStepperIsActive(&fs_stepper_engine) || fsSetup_stepper_is_active(setup_engine) || fsHelp_stepper_is_active(help_engine);
}

signed int fsFileSelect(void)
{
	signed int result;
	struct fsSetup_stepper_engine *setup_engine = fsSetup_stepper_get_engine();
	struct fsHelp_stepper_engine *help_engine = fsHelp_stepper_get_engine();

	/* Help must run instead of the selector, or the next selector frame paints over it. */
	if (fsHelp_stepper_is_active(help_engine))
	{
		if (!fsHelp_stepper_run(help_engine))
			return 0;
		fsHelp_stepper_finish(help_engine);
		plSetTextMode(plScrType);
	}

	/* If setup stepper is active, drive it instead of file selector */
	if (fsSetup_stepper_is_active(setup_engine))
	{
		if (fsSetup_stepper_run(setup_engine))
		{
			/* Setup finished - cleanup and return to file selector */
			fsSetup_stepper_finish(setup_engine);
			plSetTextMode(plScrType);
			fsScanDir(0);
		}
		/* Return 0 to indicate "still running" */
		return 0;
	}

	/* Install the catalog built by wasm/build.sh once IndexedDB has been read. */
	{
		extern int wasm_modland_seed_is_active(void);
		extern int wasm_modland_seed_tick(const struct DevInterfaceAPI_t *API);
		if (wasm_modland_seed_is_active())
		{
			wasm_modland_seed_tick(&DevInterfaceAPI);
		}
	}

	/* Normal file selector logic */
	if (!fsFileSelectStepperIsActive(&fs_stepper_engine))
	{
		fsFileSelectStepperReset(&fs_stepper_engine);
	}

	result = fsFileSelectStepperRun(&fs_stepper_engine, 1);

	if (result != FS_FILESELECT_RESULT_IN_PROGRESS)
	{
		fsFileSelectStepperReset(&fs_stepper_engine);
		return result;
	}

	return 0;
}

/* Global state tracking for virtual device steppers
 * Side modules (like modland.wasm) can set this to indicate they're active.
 * This avoids the linking issue of calling functions in side modules from main module.
 *
 * IMPORTANT: We use a function-based API instead of a direct global variable
 * because WASM side modules cannot directly modify main module globals.
 */
static int g_virtual_device_stepper_active_internal = 0;

/* Function-based API for side modules to access the stepper state
 * These functions are exported so side modules can call them
 */
EMSCRIPTEN_KEEPALIVE
int wasm_get_virtual_device_stepper_active(void) {
	return g_virtual_device_stepper_active_internal;
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_virtual_device_stepper_active(int value) {
	g_virtual_device_stepper_active_internal = value;
}

/* WASM-aware VirtualInterfaceRun that checks stepper state
 * The original VirtualInterfaceRun() calls DevInterface_Run() which always
 * returns interfaceReturnNextAuto. This wrapper checks if any stepper
 * is active and returns Continue to keep the dialog alive.
 */
static interfaceReturnEnum VirtualInterfaceRun_stepper_wrapper(void)
{
	interfaceReturnEnum result = VirtualInterfaceRun_blocking();

	/* If any stepper is active, override NextAuto with Continue */
	if (result == interfaceReturnNextAuto && g_virtual_device_stepper_active_internal)
	{
		return interfaceReturnContinue;
	}

	return result;
}

/* Patch the VirtualInterface struct to use our stepper-aware wrapper
 * This is called from fsInit() which is defined in the included pfilesel.c.
 * We wrap fsInit to do the patching after the original init completes.
 */

int fsInit(void)
{
	int result;

	/* First call the original fsInit which registers VirtualInterface */
	result = fsInit_blocking();

	if (result)
	{
		/* Now patch VirtualInterface.Run to use our stepper-aware wrapper */
		VirtualInterface.Run = VirtualInterfaceRun_stepper_wrapper;
	}

	return result;
}

#else
signed int fsFileSelect(void)
{
	return fsFileSelect_blocking();
}

int fsFileSelectIsActive(void)
{
	return 0;
}
#endif

/* WASM-specific fsSetup that uses stepper architecture
 * The original fsSetup() has a blocking keyboard loop that hangs in WASM.
 * This version starts a stepper that runs from the file selector main loop.
 */

/* Check if setup stepper is active */
static int fsSetup_is_stepper_active(void)
{
	return fsSetup_stepper_is_active(fsSetup_stepper_get_engine());
}

void fsSetup(void)
{
	struct fsSetup_stepper_engine *engine = fsSetup_stepper_get_engine();

	/* Just initialize the stepper and return immediately
	 * The stepper will be driven by fsFileSelectStepperRun()
	 */
	fsSetup_stepper_init(engine);
}
