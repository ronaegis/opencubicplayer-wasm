/* Stepper infrastructure for non-blocking operation in WASM builds */

enum modland_com_cachedir_state {
	CACHEDIR_STATE_NORMAL = 0,
	CACHEDIR_STATE_EDITING = 1
};

struct modland_com_cachedir_engine {
	int is_active;
	int first_run;
	int selected;
	int origselected;
	int quit;
	enum modland_com_cachedir_state state;
	int edit_quit;
	char *home_modland_com;
	char *ocpdatahome_modland_com;
	char *ocpdata_modland_com;
	char *temp_modland_com;
	char *custom_modland_com;
};

static void modland_com_cachedir_engine_reset(struct modland_com_cachedir_engine *engine)
{
	if (!engine)
	{
		return;
	}

	if (engine->home_modland_com)
	{
		free(engine->home_modland_com);
		engine->home_modland_com = NULL;
	}
	if (engine->ocpdatahome_modland_com)
	{
		free(engine->ocpdatahome_modland_com);
		engine->ocpdatahome_modland_com = NULL;
	}
	if (engine->ocpdata_modland_com)
	{
		free(engine->ocpdata_modland_com);
		engine->ocpdata_modland_com = NULL;
	}
	if (engine->temp_modland_com)
	{
		free(engine->temp_modland_com);
		engine->temp_modland_com = NULL;
	}
	if (engine->custom_modland_com)
	{
		free(engine->custom_modland_com);
		engine->custom_modland_com = NULL;
	}

	engine->is_active = 0;
	engine->first_run = 0;
	engine->selected = 0;
	engine->origselected = 0;
	engine->quit = 0;
	engine->state = CACHEDIR_STATE_NORMAL;
	engine->edit_quit = 0;
}

static void modland_com_cachedir_engine_finish(struct modland_com_cachedir_engine *engine)
{
	modland_com_cachedir_engine_reset(engine);
}

static void modland_com_cachedir_Run_impl(
	struct modland_com_cachedir_engine *engine,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
)
{
	int first_call;
	int loop_limit = (iteration_limit > 0) ? iteration_limit : -1;

	if (!engine)
	{
		if (result) *result = -1;
		return;
	}

	if (!engine->is_active)
	{
		engine->is_active = 1;
		engine->first_run = 1;
		engine->quit = 0;
		engine->state = CACHEDIR_STATE_NORMAL;
		engine->edit_quit = 0;

		/* Allocate path strings */
		engine->home_modland_com = modland_com_resolve_cachedir2(API->configAPI->HomePath, "modland.com");
		engine->ocpdatahome_modland_com = modland_com_resolve_cachedir2(API->configAPI->DataHomePath, "modland.com");
		engine->ocpdata_modland_com = modland_com_resolve_cachedir2(API->configAPI->DataPath, "modland.com");
		engine->temp_modland_com = modland_com_resolve_cachedir2(API->configAPI->TempPath, "modland.com");
		engine->custom_modland_com = modland_com_resolve_cachedir(API->configAPI, modland_com.cacheconfigcustom);

		/* Determine current selection */
		if (((!strncmp(modland_com.cacheconfig, "~\\", 2)) ||
		     (!strncmp(modland_com.cacheconfig, "~/", 2))) &&
		    (!strcmp(modland_com.cacheconfig + 2, "modland.com" DIRSEPARATOR)))
		{
			engine->selected = 1;
		} else if (((!strncmp(modland_com.cacheconfig, "$HOME\\", 6)) ||
		            (!strncmp(modland_com.cacheconfig, "$HOME/", 6))) &&
		           (!strcmp(modland_com.cacheconfig + 6, "modland.com" DIRSEPARATOR)))
		{
			engine->selected = 1;
		} else if (((!strncmp(modland_com.cacheconfig, "$OCPDATAHOME\\", 13)) ||
		            (!strncmp(modland_com.cacheconfig, "$OCPDATAHOME/", 13))) &&
		           (!strcmp(modland_com.cacheconfig + 13, "modland.com" DIRSEPARATOR)))
		{
			engine->selected = 0;
		} else if (((!strncmp(modland_com.cacheconfig, "$OCPDATA\\", 9)) ||
		            (!strncmp(modland_com.cacheconfig, "$OCPDATA/", 9))) &&
		           (!strcmp(modland_com.cacheconfig + 9, "modland.com" DIRSEPARATOR)))
		{
			engine->selected = 2;
		} else if (((!strncmp(modland_com.cacheconfig, "$TEMP\\", 6)) ||
		            (!strncmp(modland_com.cacheconfig, "$TEMP/", 6))) &&
		           (!strcmp(modland_com.cacheconfig + 6, "modland.com" DIRSEPARATOR)))
		{
			engine->selected = 3;
		} else {
			engine->selected = 4;
			free(modland_com.cacheconfigcustom);
			modland_com.cacheconfigcustom = strdup(modland_com.cacheconfig);
		}

		engine->origselected = engine->selected;
	}
	first_call = engine->first_run;

	if (first_call)
	{
		engine->first_run = 0;
	}

	for (int iter = 0; (loop_limit < 0) || (iter < loop_limit); iter++)
	{
		if (engine->state == CACHEDIR_STATE_EDITING)
		{
			/* Handle custom cachedir editing */
			modland_com_cachedir_Draw(
				API->console,
				engine->origselected,
				engine->selected,
				engine->ocpdatahome_modland_com,
				engine->home_modland_com,
				engine->ocpdata_modland_com,
				engine->temp_modland_com,
				engine->custom_modland_com,
				&modland_com.cacheconfigcustom,
				&engine->edit_quit
			);
			API->console->FrameLock();

			if (engine->edit_quit)
			{
				engine->state = CACHEDIR_STATE_NORMAL;
				engine->edit_quit = 0;
			}
			continue;
		}

		/* Draw normal cachedir selection dialog */
		API->fsDraw();
		modland_com_cachedir_Draw(
			API->console,
			engine->origselected,
			engine->selected,
			engine->ocpdatahome_modland_com,
			engine->home_modland_com,
			engine->ocpdata_modland_com,
			engine->temp_modland_com,
			engine->custom_modland_com,
			&modland_com.cacheconfigcustom,
			0
		);

		/* Handle keyboard input */
		while (API->console->KeyboardHit() && !engine->quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
					modland_com_cachedir_engine_finish(engine);
					if (result) *result = 0;
					return;

				case KEY_ESC:
					engine->quit = 1;
					break;

				case KEY_UP:
					if (engine->selected)
					{
						engine->selected--;
					}
					break;

				case KEY_DOWN:
					if (engine->selected < 4)
					{
						engine->selected++;
					}
					break;

				case ' ':
					engine->origselected = engine->selected;
					modland_com_cachedir_Save(API, engine->selected, &engine->custom_modland_com);
					break;

				case _KEY_ENTER:
					engine->origselected = engine->selected;
					if (engine->selected == 4)
					{
						engine->state = CACHEDIR_STATE_EDITING;
						engine->edit_quit = 0;
					}
					modland_com_cachedir_Save(API, engine->selected, &engine->custom_modland_com);
					break;
			}
		}

		if (engine->quit)
		{
			modland_com_cachedir_engine_finish(engine);
			if (result) *result = 1;
			return;
		}

		API->console->FrameLock();
	}

	/* Still in progress */
	if (result) *result = 2;
}

/* Public stepper API */
void modland_com_cachedir_StepperReset(struct modland_com_cachedir_engine *engine)
{
	modland_com_cachedir_engine_reset(engine);
}

int modland_com_cachedir_StepperIsActive(const struct modland_com_cachedir_engine *engine)
{
	return engine && engine->is_active;
}

void modland_com_cachedir_StepperRun(
	struct modland_com_cachedir_engine *engine,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
)
{
	modland_com_cachedir_Run_impl(engine, API, iteration_limit, result);
}

