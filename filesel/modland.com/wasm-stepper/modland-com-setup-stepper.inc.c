/* Stepper infrastructure for non-blocking operation in WASM builds */

enum modland_com_setup_subdialog_state {
	SETUP_SUBDIALOG_NONE = 0,
	SETUP_SUBDIALOG_MIRROR = 1,
	SETUP_SUBDIALOG_INITIALIZE = 2,
	SETUP_SUBDIALOG_CACHEDIR = 3,
	SETUP_SUBDIALOG_WIPECACHE = 4
};

struct modland_com_setup_engine {
	int is_active;
	int first_run;
	int selected;
	int quit;
	enum modland_com_setup_subdialog_state subdialog;
};

static void modland_com_setup_engine_reset(struct modland_com_setup_engine *engine)
{
	if (!engine)
	{
		return;
	}
	engine->is_active = 0;
	engine->first_run = 0;
	engine->selected = 0;
	engine->quit = 0;
	engine->subdialog = SETUP_SUBDIALOG_NONE;
}

static void modland_com_setup_engine_finish(struct modland_com_setup_engine *engine)
{
	modland_com_setup_engine_reset(engine);
}

/* Forward declarations for subdialog steppers */
struct modland_com_mirror_engine;
struct modland_com_initialize_engine;
struct modland_com_cachedir_engine;
struct modland_com_wipecache_engine;

extern void modland_com_mirror_StepperRun(
	struct modland_com_mirror_engine *engine,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
);
extern int modland_com_mirror_StepperIsActive(const struct modland_com_mirror_engine *engine);
extern void *modland_com_mirror_StepperGetEngine(void);

extern void modland_com_initialize_StepperRun(
	struct modland_com_initialize_engine *engine,
	void **token,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
);
extern int modland_com_initialize_StepperIsActive(const struct modland_com_initialize_engine *engine);
extern void *modland_com_initialize_StepperGetEngine(void);

extern void modland_com_cachedir_StepperRun(
	struct modland_com_cachedir_engine *engine,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
);
extern int modland_com_cachedir_StepperIsActive(const struct modland_com_cachedir_engine *engine);
extern void *modland_com_cachedir_StepperGetEngine(void);

extern void modland_com_wipecache_StepperRun(
	struct modland_com_wipecache_engine *engine,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
);
extern int modland_com_wipecache_StepperIsActive(const struct modland_com_wipecache_engine *engine);
extern void *modland_com_wipecache_StepperGetEngine(void);

static void modland_com_setup_Run_impl(
	struct modland_com_setup_engine *engine,
	void **token,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit
)
{
	int first_call;
	int loop_limit = (iteration_limit > 0) ? iteration_limit : -1;
	int subdialog_result = 0;

	if (engine)
	{
		if (!engine->is_active)
		{
			engine->is_active = 1;
			engine->first_run = 1;
			engine->selected = 0;
			engine->quit = 0;
			engine->subdialog = SETUP_SUBDIALOG_NONE;
		}
		first_call = engine->first_run;
	} else {
		first_call = 1;
	}

	if (first_call && engine)
	{
		engine->first_run = 0;
	}

	for (int iter = 0; (loop_limit < 0) || (iter < loop_limit); iter++)
	{
		/* Handle active subdialogs */
		if (engine && engine->subdialog != SETUP_SUBDIALOG_NONE)
		{
			subdialog_result = 0;

			switch (engine->subdialog)
			{
				case SETUP_SUBDIALOG_MIRROR:
					if (modland_com_mirror_StepperIsActive(modland_com_mirror_StepperGetEngine()))
					{
						modland_com_mirror_StepperRun(modland_com_mirror_StepperGetEngine(), API, 1, &subdialog_result);
						return; /* Still active, call again next frame */
					}
					engine->subdialog = SETUP_SUBDIALOG_NONE;
					break;

				case SETUP_SUBDIALOG_INITIALIZE:
					if (modland_com_initialize_StepperIsActive(modland_com_initialize_StepperGetEngine()))
					{
						modland_com_initialize_StepperRun(modland_com_initialize_StepperGetEngine(), token, API, 1, &subdialog_result);
						return; /* Still active, call again next frame */
					}
					engine->subdialog = SETUP_SUBDIALOG_NONE;
					break;

				case SETUP_SUBDIALOG_CACHEDIR:
					if (modland_com_cachedir_StepperIsActive(modland_com_cachedir_StepperGetEngine()))
					{
						modland_com_cachedir_StepperRun(modland_com_cachedir_StepperGetEngine(), API, 1, &subdialog_result);
						return; /* Still active, call again next frame */
					}
					engine->subdialog = SETUP_SUBDIALOG_NONE;
					break;

				case SETUP_SUBDIALOG_WIPECACHE:
					if (modland_com_wipecache_StepperIsActive(modland_com_wipecache_StepperGetEngine()))
					{
						modland_com_wipecache_StepperRun(modland_com_wipecache_StepperGetEngine(), API, 1, &subdialog_result);
						return; /* Still active, call again next frame */
					}
					engine->subdialog = SETUP_SUBDIALOG_NONE;
					break;

				default:
					engine->subdialog = SETUP_SUBDIALOG_NONE;
					break;
			}
		}

		/* Draw the setup dialog */
		API->fsDraw();
		modland_com_setup_Draw(
			API->console,
			engine ? engine->selected : 0,
			modland_com.mirror,
			modland_com.database.fileentries_n,
			modland_com.database.year,
			modland_com.database.month,
			modland_com.database.day,
			modland_com.cacheconfig,
			modland_com.cachepath,
			modland_com.showrelevantdirectoriesonly
		);

		/* Handle keyboard input */
		while (API->console->KeyboardHit() && (!engine || !engine->quit))
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
				case KEY_ESC:
					if (engine)
					{
						engine->quit = 1;
					}
					break;
				case KEY_UP:
					if (engine && engine->selected)
					{
						engine->selected--;
					}
					break;
				case KEY_DOWN:
					if (engine && engine->selected < 5)
					{
						engine->selected++;
					}
					break;
				case '1': if (engine) engine->selected = 0; break;
				case '2': if (engine) engine->selected = 1; break;
				case '3': if (engine) engine->selected = 2; break;
				case '4': if (engine) engine->selected = 3; break;
				case '5': if (engine) engine->selected = 4; break;
				case '6': if (engine) engine->selected = 5; break;
				case _KEY_ENTER:
					if (!engine)
					{
						break;
					}
					switch (engine->selected)
					{
						case 0:
							engine->subdialog = SETUP_SUBDIALOG_MIRROR;
							modland_com_mirror_StepperRun(modland_com_mirror_StepperGetEngine(), API, 1, &subdialog_result);
							return; /* Let subdialog run next frame */

						case 1:
						{
							engine->subdialog = SETUP_SUBDIALOG_INITIALIZE;
							modland_com_initialize_StepperRun(modland_com_initialize_StepperGetEngine(), token, API, 1, &subdialog_result);
							return; /* Let subdialog run next frame */
						}

						case 2:
						{
							API->fsForceNextRescan();
							modland_com_database_clear();
							if (!modland_com_filedb_save_start())
							{
								while (modland_com_filedb_save_iterate() == 1);
							}
							break;
						}

						case 3:
							engine->subdialog = SETUP_SUBDIALOG_CACHEDIR;
							modland_com_cachedir_StepperRun(modland_com_cachedir_StepperGetEngine(), API, 1, &subdialog_result);
							return; /* Let subdialog run next frame */

						case 4:
							engine->subdialog = SETUP_SUBDIALOG_WIPECACHE;
							modland_com_wipecache_StepperRun(modland_com_wipecache_StepperGetEngine(), API, 1, &subdialog_result);
							return; /* Let subdialog run next frame */

						case 5:
						{
							modland_com.showrelevantdirectoriesonly = !modland_com.showrelevantdirectoriesonly;
							API->configAPI->SetProfileBool("modland.com", "showrelevantdirectoriesonly", modland_com.showrelevantdirectoriesonly);
							API->configAPI->StoreConfig();
							break;
						}
					}
					break;
			}
		}

		if (engine && engine->quit)
		{
			modland_com_setup_engine_finish(engine);
			return;
		}
	}
}

/* Public stepper API */
void modland_com_setup_StepperReset(struct modland_com_setup_engine *engine)
{
	modland_com_setup_engine_reset(engine);
}

int modland_com_setup_StepperIsActive(const struct modland_com_setup_engine *engine)
{
	return engine && engine->is_active;
}

void modland_com_setup_StepperRun(
	struct modland_com_setup_engine *engine,
	void **token,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit
)
{
	modland_com_setup_Run_impl(engine, token, API, iteration_limit);
}

