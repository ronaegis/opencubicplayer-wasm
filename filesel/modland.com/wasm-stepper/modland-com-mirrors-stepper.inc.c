/* Stepper infrastructure for non-blocking operation in WASM builds */

enum modland_com_mirror_state {
	MIRROR_STATE_NORMAL = 0,
	MIRROR_STATE_EDITING = 1
};

struct modland_com_mirror_engine {
	int is_active;
	int first_run;
	int selected;
	int origselected;
	int quit;
	enum modland_com_mirror_state state;
	int edit_quit;
};

static void modland_com_mirror_engine_reset(struct modland_com_mirror_engine *engine)
{
	if (!engine)
	{
		return;
	}
	engine->is_active = 0;
	engine->first_run = 0;
	engine->selected = 0;
	engine->origselected = 0;
	engine->quit = 0;
	engine->state = MIRROR_STATE_NORMAL;
	engine->edit_quit = 0;
}

static void modland_com_mirror_engine_finish(struct modland_com_mirror_engine *engine)
{
	modland_com_mirror_engine_reset(engine);
}

static void modland_com_mirror_Run_impl(
	struct modland_com_mirror_engine *engine,
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
		engine->state = MIRROR_STATE_NORMAL;
		engine->edit_quit = 0;

		/* Find current mirror in list */
		for (engine->selected = 0; engine->selected < NUM_MIRRORS; engine->selected++)
		{
			if (!strcasecmp(modland_com.mirror, modland_com_official_mirror[engine->selected]))
			{
				break;
			}
		}
		if (engine->selected >= NUM_MIRRORS)
		{
			free(modland_com.mirrorcustom);
			modland_com.mirrorcustom = strdup(modland_com.mirror);
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
		if (engine->state == MIRROR_STATE_EDITING)
		{
			/* Handle custom mirror editing */
			modland_com_mirror_Draw(API->console, engine->origselected, engine->selected,
			                        &modland_com.mirrorcustom, &engine->edit_quit);
			API->console->FrameLock();

			if (engine->edit_quit)
			{
				engine->state = MIRROR_STATE_NORMAL;
				engine->edit_quit = 0;
			}
			continue;
		}

		/* Draw normal mirror selection dialog */
		API->fsDraw();
		modland_com_mirror_Draw(API->console, engine->origselected, engine->selected,
		                        &modland_com.mirrorcustom, 0);

		/* Handle keyboard input */
		while (API->console->KeyboardHit() && !engine->quit)
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_EXIT:
					modland_com_mirror_engine_finish(engine);
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
					if (engine->selected < NUM_MIRRORS)
					{
						engine->selected++;
					}
					break;

				case ' ':
					engine->origselected = engine->selected;
					modland_com_mirror_Save(API, engine->selected);
					break;

				case _KEY_ENTER:
					engine->origselected = engine->selected;
					if (engine->selected == NUM_MIRRORS)
					{
						engine->state = MIRROR_STATE_EDITING;
						engine->edit_quit = 0;
					}
					modland_com_mirror_Save(API, engine->selected);
					break;
			}
		}

		if (engine->quit)
		{
			modland_com_mirror_engine_finish(engine);
			if (result) *result = 1;
			return;
		}

		API->console->FrameLock();
	}

	/* Still in progress */
	if (result) *result = 2;
}

/* Public stepper API */
void modland_com_mirror_StepperReset(struct modland_com_mirror_engine *engine)
{
	modland_com_mirror_engine_reset(engine);
}

int modland_com_mirror_StepperIsActive(const struct modland_com_mirror_engine *engine)
{
	return engine && engine->is_active;
}

void modland_com_mirror_StepperRun(
	struct modland_com_mirror_engine *engine,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
)
{
	modland_com_mirror_Run_impl(engine, API, iteration_limit, result);
}

