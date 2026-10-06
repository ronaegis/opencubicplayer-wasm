/* WASM modland.com stepper wrapper
 * This file uses the macro-include pattern to provide non-blocking wrappers
 * for the modland.com dialogs without modifying the original source files.
 */

#include "config.h"

/* Rename original blocking functions before including */
#define modland_com_init modland_com_init_blocking
#define modland_com_setup_Run modland_com_setup_Run_blocking
#define modland_com_initialize_Run modland_com_initialize_Run_blocking
#define modland_com_mirror_Run modland_com_mirror_Run_blocking
#define modland_com_cachedir_Run modland_com_cachedir_Run_blocking
#define modland_com_wipecache_Run modland_com_wipecache_Run_blocking
#define dllextinfo modland_com_dllextinfo_blocking

/* Include the original modland-com.c which includes all subdialogs */
#include "../filesel/modland.com/modland-com.c"

/* Undefine the renames */
#undef modland_com_setup_Run
#undef modland_com_initialize_Run
#undef modland_com_mirror_Run
#undef modland_com_cachedir_Run
#undef modland_com_wipecache_Run
#undef modland_com_init
#undef dllextinfo

/* Forward declarations for the init wrappers implemented later in this file */
struct PluginInitAPI_t;
int modland_com_init(struct PluginInitAPI_t *API);
int modland_com_init_blocking(struct PluginInitAPI_t *API);

/* Rebuild the exported plugin descriptor so PluginInit points at the wrapper */
DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {
	.name = "modland-com",
	.desc = "OpenCP virtual modland.com filebrowser (c) 2024-'25 Stian Skjelstad",
	.ver = DLLVERSION,
	.sortindex = 60,
#ifdef OCP_WASM_FILESEL_STEPPER
	.PluginInit = modland_com_init,
#else
	.PluginInit = modland_com_init_blocking,
#endif
	.PluginClose = modland_com_done
};

/* Function-based API for accessing main module's stepper state
 * This avoids the WASM side module global variable isolation issue
 */
extern int wasm_get_virtual_device_stepper_active(void);
extern void wasm_set_virtual_device_stepper_active(int value);

#ifdef OCP_WASM_FILESEL_STEPPER

/* Static engine instances for each dialog */
static struct modland_com_setup_engine setup_engine;
static struct modland_com_initialize_engine initialize_engine;
static struct modland_com_mirror_engine mirror_engine;
static struct modland_com_cachedir_engine cachedir_engine;
static struct modland_com_wipecache_engine wipecache_engine;

/* Forward declaration for state update function */
static void modland_com_update_stepper_state(void);
void modland_com_setup_Run(void **token, const struct DevInterfaceAPI_t *API);

struct wasm_dev_ocpfile_t
{
	struct ocpfile_t  head;
	void             *token;
	int  (*Init)       (void **token, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API);
	void (*Run)        (void **token,                                const struct DevInterfaceAPI_t *API);
	void (*Close)      (void **token,                                const struct DevInterfaceAPI_t *API);
	void (*Destructor) (void  *token);
};

static void wasm_modland_patch_virtual_interface_runs(void)
{
	struct wasm_dev_ocpfile_t *dev;

	if (modland_com.modland_com_setup)
	{
		dev = (struct wasm_dev_ocpfile_t *)modland_com.modland_com_setup;
		dev->Run = modland_com_setup_Run;
	}

	if (modland_com.setup_modland_com)
	{
		dev = (struct wasm_dev_ocpfile_t *)modland_com.setup_modland_com;
		dev->Run = modland_com_setup_Run;
	}
}

static void modland_com_update_stepper_state(void)
{
	int active = modland_com_setup_StepperIsActive(&setup_engine) ||
	             modland_com_initialize_StepperIsActive(&initialize_engine) ||
	             modland_com_mirror_StepperIsActive(&mirror_engine) ||
	             modland_com_cachedir_StepperIsActive(&cachedir_engine) ||
	             modland_com_wipecache_StepperIsActive(&wipecache_engine);

	wasm_set_virtual_device_stepper_active(active);
}

/* --- Setup Dialog --- */

void *modland_com_setup_StepperGetEngine(void)
{
	return &setup_engine;
}

void modland_com_setup_Run(void **token, const struct DevInterfaceAPI_t *API)
{
	if (!modland_com_setup_StepperIsActive(&setup_engine))
	{
		modland_com_setup_StepperReset(&setup_engine);
	}

	modland_com_setup_StepperRun(&setup_engine, token, API, 1);

	modland_com_update_stepper_state();
}

/* --- Initialize Dialog --- */

void *modland_com_initialize_StepperGetEngine(void)
{
	return &initialize_engine;
}

void modland_com_initialize_Run(void **token, const struct DevInterfaceAPI_t *API)
{
	int result = 0;
	int iteration_limit;

	if (!modland_com_initialize_StepperIsActive(&initialize_engine))
	{
		modland_com_initialize_StepperReset(&initialize_engine);
	}

	/* Use adaptive iteration limit based on current state:
	 * -1 (unlimited) for CPU-intensive states (parsing, sorting, saving)
	 *  1 (single) for I/O waits and user interaction
	 */
	iteration_limit = modland_com_initialize_StepperShouldRunFast(&initialize_engine) ? -1 : 1;

	modland_com_initialize_StepperRun(&initialize_engine, token, API, iteration_limit, &result);
	modland_com_update_stepper_state();
}

/* --- Mirror Dialog --- */

void *modland_com_mirror_StepperGetEngine(void)
{
	return &mirror_engine;
}

void modland_com_mirror_Run(const struct DevInterfaceAPI_t *API)
{
	int result = 0;

	if (!modland_com_mirror_StepperIsActive(&mirror_engine))
	{
		modland_com_mirror_StepperReset(&mirror_engine);
	}

	modland_com_mirror_StepperRun(&mirror_engine, API, 1, &result);
	modland_com_update_stepper_state();
}

/* --- Cache Directory Dialog --- */

void *modland_com_cachedir_StepperGetEngine(void)
{
	return &cachedir_engine;
}

void modland_com_cachedir_Run(const struct DevInterfaceAPI_t *API)
{
	int result = 0;

	if (!modland_com_cachedir_StepperIsActive(&cachedir_engine))
	{
		modland_com_cachedir_StepperReset(&cachedir_engine);
	}

	modland_com_cachedir_StepperRun(&cachedir_engine, API, 1, &result);
	modland_com_update_stepper_state();
}

/* --- Wipe Cache Dialog --- */

void *modland_com_wipecache_StepperGetEngine(void)
{
	return &wipecache_engine;
}

void modland_com_wipecache_Run(const struct DevInterfaceAPI_t *API)
{
	int result = 0;

	if (!modland_com_wipecache_StepperIsActive(&wipecache_engine))
	{
		modland_com_wipecache_StepperReset(&wipecache_engine);
	}

	modland_com_wipecache_StepperRun(&wipecache_engine, API, 1, &result);
	modland_com_update_stepper_state();
}

#else /* !OCP_WASM_FILESEL_STEPPER */

/* Desktop build: use original blocking implementations */
void modland_com_setup_Run(void **token, const struct DevInterfaceAPI_t *API)
{
	modland_com_setup_Run_blocking(token, API);
}

void modland_com_initialize_Run(void **token, const struct DevInterfaceAPI_t *API)
{
	modland_com_initialize_Run_blocking(token, API);
}

void modland_com_mirror_Run(const struct DevInterfaceAPI_t *API)
{
	modland_com_mirror_Run_blocking(API);
}

void modland_com_cachedir_Run(const struct DevInterfaceAPI_t *API)
{
	modland_com_cachedir_Run_blocking(API);
}

void modland_com_wipecache_Run(const struct DevInterfaceAPI_t *API)
{
	modland_com_wipecache_Run_blocking(API);
}

/* Global state variable in main module (defined in pfilesel-stepper.c)
 * We set this to indicate that a modland stepper is active.
 * The main module's VirtualInterfaceRun wrapper checks this to keep dialog alive.
 */
#endif /* OCP_WASM_FILESEL_STEPPER */

int modland_com_init(struct PluginInitAPI_t *API)
{
	int result = modland_com_init_blocking(API);

#ifdef OCP_WASM_FILESEL_STEPPER
	if (result == errOk)
	{
		wasm_modland_patch_virtual_interface_runs();

		modland_com_update_stepper_state();
	}
	else
	{
		wasm_set_virtual_device_stepper_active(0);
	}
#endif

	return result;
}
