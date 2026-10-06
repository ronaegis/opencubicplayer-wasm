/* Stepper infrastructure for non-blocking operation in WASM builds */

enum modland_com_wipecache_state {
	WIPECACHE_STATE_IDLE = 0,
	WIPECACHE_STATE_COUNTING,
	WIPECACHE_STATE_MENU,
	WIPECACHE_STATE_DELETING,
	WIPECACHE_STATE_COMPLETE,
	WIPECACHE_STATE_DONE
};

struct modland_com_wipecache_engine {
	int is_active;
	int first_run;
	enum modland_com_wipecache_state state;
	int selected;
	int can_recycle;
	int quit;
	struct osdir_size_t size_ctx;
	struct osdir_delete_t delete_ctx;
	uint_fast32_t directories_target_n;
	uint_fast32_t files_target_n;
};

static void modland_com_wipecache_engine_reset(struct modland_com_wipecache_engine *engine)
{
	if (!engine)
	{
		return;
	}

	memset(&engine->size_ctx, 0, sizeof(engine->size_ctx));
	memset(&engine->delete_ctx, 0, sizeof(engine->delete_ctx));

	engine->is_active = 0;
	engine->first_run = 0;
	engine->state = WIPECACHE_STATE_IDLE;
	engine->selected = 2;
	engine->can_recycle = 0;
	engine->quit = 0;
	engine->directories_target_n = 0;
	engine->files_target_n = 0;
}

static void modland_com_wipecache_engine_finish(struct modland_com_wipecache_engine *engine)
{
	modland_com_wipecache_engine_reset(engine);
}

static void modland_com_wipecache_Run_impl(
	struct modland_com_wipecache_engine *engine,
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
		engine->selected = 2;
		engine->can_recycle = osdir_trash_available(modland_com.cachepath);
		memset(&engine->size_ctx, 0, sizeof(engine->size_ctx));
		memset(&engine->delete_ctx, 0, sizeof(engine->delete_ctx));

		if (osdir_size_start(&engine->size_ctx, modland_com.cachepath))
		{
			engine->state = WIPECACHE_STATE_MENU;
		} else {
			engine->state = WIPECACHE_STATE_COUNTING;
		}
	}
	first_call = engine->first_run;

	if (first_call)
	{
		engine->first_run = 0;
	}

	for (int iter = 0; (loop_limit < 0) || (iter < loop_limit); iter++)
	{
		switch (engine->state)
		{
			case WIPECACHE_STATE_COUNTING:
			{
				/* Calculate directory size */
				API->fsDraw();
				modland_com_wipecache_Draw(
					API->console,
					engine->selected,
					modland_com.cacheconfig,
					modland_com.cachepath,
					engine->size_ctx.directories_n,
					engine->size_ctx.files_n,
					engine->size_ctx.files_size,
					1,
					engine->can_recycle
				);

				/* Check for cancel */
				while (API->console->KeyboardHit())
				{
					int key = API->console->KeyboardGetChar();
					if (key == KEY_EXIT || key == KEY_ESC)
					{
						osdir_size_cancel(&engine->size_ctx);
						modland_com_wipecache_engine_finish(engine);
						if (result) *result = 0;
						return;
					}
					if (key == KEY_LEFT)
					{
						if (engine->selected)
						{
							engine->selected--;
							if ((!engine->can_recycle) && (engine->selected == 1))
							{
								engine->selected--;
							}
						}
					}
					if (key == KEY_RIGHT)
					{
						if (engine->selected < 2)
						{
							engine->selected++;
							if ((!engine->can_recycle) && (engine->selected == 1))
							{
								engine->selected++;
							}
						}
					}
				}

				/* Iterate size calculation (batch mode) */
				int still_counting = 1;
				do
				{
					if (!osdir_size_iterate(&engine->size_ctx))
					{
						still_counting = 0;
						engine->state = WIPECACHE_STATE_MENU;
						engine->directories_target_n = engine->size_ctx.directories_n;
						engine->files_target_n = engine->size_ctx.files_n;
						break;
					}
				} while (!API->console->PollFrameLock());

				if (!still_counting)
				{
					break;
				}
				break;
			}

			case WIPECACHE_STATE_MENU:
			{
				/* Display menu and wait for selection */
				API->fsDraw();
				modland_com_wipecache_Draw(
					API->console,
					engine->selected,
					modland_com.cacheconfig,
					modland_com.cachepath,
					engine->size_ctx.directories_n,
					engine->size_ctx.files_n,
					engine->size_ctx.files_size,
					0,
					engine->can_recycle
				);

				while (API->console->KeyboardHit())
				{
					int key = API->console->KeyboardGetChar();
					switch (key)
					{
						case KEY_EXIT:
						case KEY_ESC:
							modland_com_wipecache_engine_finish(engine);
							if (result) *result = 0;
							return;

						case KEY_LEFT:
							if (engine->selected)
							{
								engine->selected--;
								if ((!engine->can_recycle) && (engine->selected == 1))
								{
									engine->selected--;
								}
							}
							break;

						case KEY_RIGHT:
							if (engine->selected < 2)
							{
								engine->selected++;
								if ((!engine->can_recycle) && (engine->selected == 1))
								{
									engine->selected++;
								}
							}
							break;

						case _KEY_ENTER:
							if (engine->selected == 0)
							{
								/* Start deletion */
								if (!osdir_delete_start(&engine->delete_ctx, modland_com.cachepath))
								{
									engine->state = WIPECACHE_STATE_DELETING;
								} else {
									engine->state = WIPECACHE_STATE_COMPLETE;
								}
							} else if (engine->selected == 1)
							{
								/* Move to recycle bin */
								osdir_trash_perform(modland_com.cachepath);
								modland_com_wipecache_engine_finish(engine);
								if (result) *result = 1;
								return;
							} else if (engine->selected == 2)
							{
								/* Cancel */
								modland_com_wipecache_engine_finish(engine);
								if (result) *result = 0;
								return;
							}
							break;
					}
				}

				API->console->FrameLock();
				break;
			}

			case WIPECACHE_STATE_DELETING:
			{
				/* Perform deletion */
				API->fsDraw();
				modland_com_dowipecache_Draw(
					API->console,
					modland_com.cacheconfig,
					modland_com.cachepath,
					engine->delete_ctx.removed_directories_n + engine->delete_ctx.failed_directories_n,
					engine->directories_target_n,
					engine->delete_ctx.failed_directories_n,
					engine->delete_ctx.removed_files_n + engine->delete_ctx.failed_files_n,
					engine->files_target_n,
					engine->delete_ctx.failed_files_n,
					1
				);

				/* Check for cancel */
				while (API->console->KeyboardHit())
				{
					int key = API->console->KeyboardGetChar();
					if (key == KEY_EXIT || key == KEY_ESC)
					{
						osdir_delete_cancel(&engine->delete_ctx);
						modland_com_wipecache_engine_finish(engine);
						if (result) *result = 0;
						return;
					}
				}

				/* Iterate deletion (batch mode) */
				int still_deleting = 1;
				do
				{
					if (!osdir_delete_iterate(&engine->delete_ctx))
					{
						still_deleting = 0;
						engine->state = WIPECACHE_STATE_COMPLETE;
						break;
					}
				} while (!API->console->PollFrameLock());

				if (!still_deleting)
				{
					break;
				}
				break;
			}

			case WIPECACHE_STATE_COMPLETE:
			{
				/* Show completion dialog */
				API->fsDraw();
				modland_com_dowipecache_Draw(
					API->console,
					modland_com.cacheconfig,
					modland_com.cachepath,
					engine->delete_ctx.removed_directories_n + engine->delete_ctx.failed_directories_n,
					engine->directories_target_n,
					engine->delete_ctx.failed_directories_n,
					engine->delete_ctx.removed_files_n + engine->delete_ctx.failed_files_n,
					engine->files_target_n,
					engine->delete_ctx.failed_files_n,
					0
				);

				while (API->console->KeyboardHit())
				{
					int key = API->console->KeyboardGetChar();
					if (key == _KEY_ENTER || key == KEY_EXIT || key == KEY_ESC)
					{
						modland_com_wipecache_engine_finish(engine);
						if (result) *result = 1;
						return;
					}
				}

				API->console->FrameLock();
				break;
			}

			case WIPECACHE_STATE_DONE:
				modland_com_wipecache_engine_finish(engine);
				if (result) *result = 1;
				return;

			default:
				engine->state = WIPECACHE_STATE_IDLE;
				break;
		}
	}

	/* Still in progress */
	if (result) *result = 2;
}

/* Public stepper API */
void modland_com_wipecache_StepperReset(struct modland_com_wipecache_engine *engine)
{
	modland_com_wipecache_engine_reset(engine);
}

int modland_com_wipecache_StepperIsActive(const struct modland_com_wipecache_engine *engine)
{
	return engine && engine->is_active;
}

void modland_com_wipecache_StepperRun(
	struct modland_com_wipecache_engine *engine,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
)
{
	modland_com_wipecache_Run_impl(engine, API, iteration_limit, result);
}

