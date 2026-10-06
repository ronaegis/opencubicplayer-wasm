/* Stepper infrastructure for non-blocking operation in WASM builds */

enum modland_com_initialize_state {
	INIT_STATE_IDLE = 0,
	INIT_STATE_SPAWN_DOWNLOAD,
	INIT_STATE_DOWNLOADING,
	INIT_STATE_OPEN_ZIP,
	INIT_STATE_PARSING,
	INIT_STATE_SORTING,
	INIT_STATE_SAVING,
	INIT_STATE_WAIT_FOR_KEY,
	INIT_STATE_DONE,
	INIT_STATE_ERROR
};

struct modland_com_initialize_engine {
	int is_active;
	int first_run;
	enum modland_com_initialize_state state;

	/* Download state */
	struct download_request_t *download;
	char *url;

	/* Parse state */
	struct ocpfilehandle_t *allmods_zip_fh;
	struct ocpdir_t *allmods_zip_dir;
	uint32_t allmods_txt_dirdb_ref;
	struct ocpfile_t *allmods_txt_file;
	struct ocpfilehandle_t *allmods_txt_fh;
	struct textfile_t *allmods_txt_textfile;
	int parse_counter;
	int parse_batch_size;
	int parse_batches_since_ui_update;  /* Track batches since last UI update */
	struct modland_com_initialize_t parse_state;

	/* Save state */
	int save_complete;
	char save_message[128];
	int save_batch_size;
	int save_iterations_since_ui_update;  /* Track iterations since last UI update */

	/* Results / display */
	int year, month, day;
	int download_size;
	char error_message[256];
	int spinner_counter;  /* For rotating animation */
};

static void modland_com_initialize_engine_reset(struct modland_com_initialize_engine *engine)
{
	if (!engine)
	{
		return;
	}

	/* Clean up any active resources */
	if (engine->allmods_txt_textfile)
	{
		textfile_stop(engine->allmods_txt_textfile);
		engine->allmods_txt_textfile = NULL;
	}
	if (engine->allmods_txt_fh)
	{
		engine->allmods_txt_fh->unref(engine->allmods_txt_fh);
		engine->allmods_txt_fh = NULL;
	}
	if (engine->allmods_txt_file)
	{
		engine->allmods_txt_file->unref(engine->allmods_txt_file);
		engine->allmods_txt_file = NULL;
	}
	if (engine->allmods_txt_dirdb_ref)
	{
		dirdbUnref(engine->allmods_txt_dirdb_ref, dirdb_use_file);
		engine->allmods_txt_dirdb_ref = 0;
	}
	if (engine->allmods_zip_dir)
	{
		engine->allmods_zip_dir->unref(engine->allmods_zip_dir);
		engine->allmods_zip_dir = NULL;
	}
	if (engine->allmods_zip_fh)
	{
		engine->allmods_zip_fh->unref(engine->allmods_zip_fh);
		engine->allmods_zip_fh = NULL;
	}
	if (engine->download)
	{
		download_request_free(engine->download);
		engine->download = NULL;
	}
	if (engine->url)
	{
		free(engine->url);
		engine->url = NULL;
	}

	engine->is_active = 0;
	engine->first_run = 0;
	engine->state = INIT_STATE_IDLE;
	engine->parse_counter = 0;
	engine->parse_batch_size = 2000;
	engine->parse_batches_since_ui_update = 0;
	engine->save_batch_size = 64;
	engine->save_iterations_since_ui_update = 0;
	memset(&engine->parse_state, 0, sizeof(engine->parse_state));
	engine->save_complete = 0;
	engine->save_message[0] = '\0';
	engine->year = 0;
	engine->month = 0;
	engine->day = 0;
	engine->download_size = 0;
	engine->error_message[0] = '\0';
	engine->spinner_counter = 0;
}

static void modland_com_initialize_engine_finish(struct modland_com_initialize_engine *engine)
{
	modland_com_initialize_engine_reset(engine);
}

static void modland_com_initialize_Run_impl(
	struct modland_com_initialize_engine *engine,
	void **token,
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
		engine->state = INIT_STATE_SPAWN_DOWNLOAD;
		engine->parse_batch_size = 2000;
		engine->save_batch_size = 64;
		API->fsForceNextRescan();
	}
	first_call = engine->first_run;

	if (first_call)
	{
		engine->first_run = 0;
	}

	for (int iter = 0; (loop_limit < 0) || (iter < loop_limit); )
	{
		enum modland_com_initialize_state old_state = engine->state;
		int did_work = 1;  /* Assume we did work unless proven otherwise */

		switch (engine->state)
		{
			case INIT_STATE_SPAWN_DOWNLOAD:
			{
				/* Create download request */
				int len = strlen(modland_com.mirror ? modland_com.mirror : "") + 11 + 1;
				engine->url = malloc(len);
				if (!engine->url)
				{
					snprintf(engine->error_message, sizeof(engine->error_message), "malloc() URL failed");
					engine->state = INIT_STATE_ERROR;
					break;
				}
				snprintf(engine->url, len, "%sallmods.zip", modland_com.mirror ? modland_com.mirror : "");
				engine->download = download_request_spawn(API->configAPI, 0, engine->url);
				if (!engine->download)
				{
					snprintf(engine->error_message, sizeof(engine->error_message), "Failed to create process");
					engine->state = INIT_STATE_ERROR;
					break;
				}
				engine->state = INIT_STATE_DOWNLOADING;
				break;
			}

			case INIT_STATE_DOWNLOADING:
			{
				/* Poll download progress */
				if (!download_request_iterate(engine->download))
				{
					/* Download complete - check for errors */
					if (engine->download->errmsg)
					{
						snprintf(engine->error_message, sizeof(engine->error_message), "%s", engine->download->errmsg);
						engine->state = INIT_STATE_ERROR;
						break;
					}
					engine->download_size = engine->download->ContentLength;
					engine->year = engine->download->Year;
					engine->month = engine->download->Month;
					engine->day = engine->download->Day;
					engine->state = INIT_STATE_OPEN_ZIP;
				} else {
					/* Still downloading - draw progress */
					API->console->FrameLock();
					API->fsDraw();
					modland_com_initialize_Draw(
						API->console,
						1, NULL, engine->download->ContentLength, 0, 0, 0,
						0, NULL, 0, 0, 0,
						0, NULL, 0, 0, engine->spinner_counter++,
						2, 0
					);
					/* Check for cancel */
					while (API->console->KeyboardHit())
					{
						int key = API->console->KeyboardGetChar();
						if (key == KEY_EXIT || key == KEY_ESC || key == _KEY_ENTER)
						{
							download_request_cancel(engine->download);
							modland_com_initialize_engine_finish(engine);
							if (result) *result = 0;
							return;
						}
					}
				}
				break;
			}

			case INIT_STATE_OPEN_ZIP:
			{
				/* Show UI feedback that download is complete */
				API->console->FrameLock();
				API->fsDraw();
				modland_com_initialize_Draw(
					API->console,
					2, NULL, engine->download_size, engine->year, engine->month, engine->day,
					0, NULL, 0, 0, 0,
					0, NULL, 0, 0, engine->spinner_counter,
					2, 0
				);

				/* Get file handle from download */
				engine->allmods_zip_fh = download_request_getfilehandle(engine->download);
				if (!engine->allmods_zip_fh)
				{
					snprintf(engine->error_message, sizeof(engine->error_message), "Unable to open the .ZIP file");
					engine->state = INIT_STATE_ERROR;
					break;
				}

				/* Open ZIP file */
				engine->allmods_zip_dir = ocpdirdecompressor_check(engine->allmods_zip_fh->origin, ".zip");
				engine->allmods_zip_fh->unref(engine->allmods_zip_fh);
				engine->allmods_zip_fh = NULL;

				if (!engine->allmods_zip_dir)
				{
					snprintf(engine->error_message, sizeof(engine->error_message), "File is not a valid .ZIP file");
					engine->state = INIT_STATE_ERROR;
					break;
				}

				/* Locate allmods.txt inside ZIP */
				engine->allmods_txt_dirdb_ref = dirdbFindAndRef(engine->allmods_zip_dir->dirdb_ref, "allmods.txt", dirdb_use_file);
				engine->allmods_txt_file = engine->allmods_zip_dir->readdir_file(engine->allmods_zip_dir, engine->allmods_txt_dirdb_ref);
				engine->allmods_zip_dir->unref(engine->allmods_zip_dir);
				engine->allmods_zip_dir = NULL;
				dirdbUnref(engine->allmods_txt_dirdb_ref, dirdb_use_file);
				engine->allmods_txt_dirdb_ref = 0;

				if (!engine->allmods_txt_file)
				{
					snprintf(engine->error_message, sizeof(engine->error_message), "Failed to locate allmods.txt inside allmods.zip");
					engine->state = INIT_STATE_ERROR;
					break;
				}

				/* Open allmods.txt */
				engine->allmods_txt_fh = engine->allmods_txt_file->open(engine->allmods_txt_file);
				engine->allmods_txt_file->unref(engine->allmods_txt_file);
				engine->allmods_txt_file = NULL;

				if (!engine->allmods_txt_fh)
				{
					snprintf(engine->error_message, sizeof(engine->error_message), "Failed to open allmods.txt inside allmods.zip");
					engine->state = INIT_STATE_ERROR;
					break;
				}

				/* Clear database and prepare for parsing */
				modland_com_database_clear();
				modland_com.database.year = engine->year;
				modland_com.database.month = engine->month;
				modland_com.database.day = engine->day;

				/* Open as textfile */
				engine->allmods_txt_textfile = textfile_start(engine->allmods_txt_fh);
				engine->allmods_txt_fh->unref(engine->allmods_txt_fh);
				engine->allmods_txt_fh = NULL;

				if (!engine->allmods_txt_textfile)
				{
					snprintf(engine->error_message, sizeof(engine->error_message), "Failed to open allmods.txt as textfile");
					engine->state = INIT_STATE_ERROR;
					break;
				}

				memset(&engine->parse_state, 0, sizeof(engine->parse_state));
				engine->parse_counter = 0;
				engine->parse_batches_since_ui_update = 0;
				engine->state = INIT_STATE_PARSING;
				break;
			}

			case INIT_STATE_PARSING:
			{
				/* Show UI feedback at start of parsing */
				if (engine->parse_counter == 0)
				{
					API->console->FrameLock();
					API->fsDraw();
					modland_com_initialize_Draw(
						API->console,
						2, NULL, engine->download_size, engine->year, engine->month, engine->day,
						1, NULL, 0, 0, 0,
						0, NULL, 0, 0, engine->spinner_counter,
						2, 0
					);
				}

				/* Mark that we've started parsing */
				engine->parse_counter++;

				/* Parse a batch of lines per iteration */
				int lines_parsed = 0;
				const char *line;

				while (lines_parsed < engine->parse_batch_size)
				{
					line = textfile_fgets(engine->allmods_txt_textfile);
					if (!line)
					{
						/* Parsing complete */
						textfile_stop(engine->allmods_txt_textfile);
						engine->allmods_txt_textfile = NULL;
						engine->state = INIT_STATE_SORTING;
						break;
					}

					if (strlen(line) > 0)
					{
						char *end;
						long filesize = strtol(line, &end, 10);
						if (end != line && filesize > 0)
						{
							line = end;
							while ((*line == '\t') || (*line == ' ')) line++;
							modland_com_add_data_line(&engine->parse_state, line, filesize);
						}
					}
					lines_parsed++;
				}

				/* Increment batch counter */
				engine->parse_batches_since_ui_update++;

				/* Force UI update every 15 batches (roughly once per second) OR if frame is ready */
				int should_update = (engine->parse_batches_since_ui_update >= 15) || API->console->PollFrameLock();

				if (should_update)
				{
					engine->parse_batches_since_ui_update = 0;
					API->console->FrameLock();
					API->fsDraw();
					modland_com_initialize_Draw(
						API->console,
						2, NULL, engine->download_size, engine->year, engine->month, engine->day,
						1, NULL, modland_com.database.fileentries_n, modland_com.database.direntries_n, engine->parse_state.invalid_entries,
						0, NULL, 0, 0, engine->spinner_counter++,
						2, 0
					);

					/* Check for cancel */
					while (API->console->KeyboardHit())
					{
						int key = API->console->KeyboardGetChar();
						if (key == KEY_EXIT || key == KEY_ESC || key == _KEY_ENTER)
						{
							modland_com_database_clear();
							modland_com_initialize_engine_finish(engine);
							if (result) *result = 0;
							return;
						}
					}
				}
				break;
			}

			case INIT_STATE_SORTING:
			{
				/* Show UI feedback that parsing is complete and sorting is starting */
				API->console->FrameLock();
				API->fsDraw();
				modland_com_initialize_Draw(
					API->console,
					2, NULL, engine->download_size, engine->year, engine->month, engine->day,
					2, NULL, modland_com.database.fileentries_n, modland_com.database.direntries_n, engine->parse_state.invalid_entries,
					0, NULL, 0, 0, engine->spinner_counter,
					2, 0
				);

				/* Sort the database */
				if (modland_com_sort())
				{
					snprintf(engine->error_message, sizeof(engine->error_message), "Out of memory");
					modland_com_database_clear();
					engine->state = INIT_STATE_ERROR;
					break;
				}

				/* Start save process */
				if (modland_com_filedb_save_start())
				{
					snprintf(engine->save_message, sizeof(engine->save_message), "Failed to initialize saving");
					engine->save_complete = 2;
					engine->state = INIT_STATE_WAIT_FOR_KEY;
				} else {
					engine->save_complete = 0;
					engine->save_iterations_since_ui_update = 0;
					engine->state = INIT_STATE_SAVING;
				}
				break;
			}

			case INIT_STATE_SAVING:
			{
				/* Show UI feedback at start of saving */
				if (engine->save_message[0] == '\0')
				{
					API->console->FrameLock();
					API->fsDraw();
					snprintf(engine->save_message, sizeof(engine->save_message), "Starting save...");
					modland_com_initialize_Draw(
						API->console,
						2, NULL, engine->download_size, engine->year, engine->month, engine->day,
						2, NULL, modland_com.database.fileentries_n, modland_com.database.direntries_n, engine->parse_state.invalid_entries,
						1, engine->save_message, 0, modland_com.database.fileentries_n, engine->spinner_counter,
						2, 0
					);
				}

				/* Increment iteration counter */
				engine->save_iterations_since_ui_update++;

				/* Force UI update every 10 batches (roughly once per second) OR if frame is ready */
				int should_update = (engine->save_iterations_since_ui_update >= 10) || API->console->PollFrameLock();

				if (should_update)
				{
					engine->save_iterations_since_ui_update = 0;
					API->console->FrameLock();
					API->fsDraw();
					snprintf(engine->save_message, sizeof(engine->save_message),
						"Written %lu of %lu file names",
						(unsigned long)(modland_com_filedb_save_f + 1),
						(unsigned long)(modland_com.database.fileentries_n));

					modland_com_initialize_Draw(
						API->console,
						2, NULL, engine->download_size, engine->year, engine->month, engine->day,
						2, NULL, modland_com.database.fileentries_n, modland_com.database.direntries_n, engine->parse_state.invalid_entries,
						1, engine->save_message,
						modland_com_filedb_save_f + 1, modland_com.database.fileentries_n, engine->spinner_counter++,
						2, 0
					);

					/* Check for cancel */
					while (API->console->KeyboardHit())
					{
						int key = API->console->KeyboardGetChar();
						if (key == KEY_EXIT || key == KEY_ESC || key == _KEY_ENTER)
						{
							modland_com_filedb_save_abort();
							engine->save_complete = 2;
							snprintf(engine->save_message, sizeof(engine->save_message), "Save aborted");
							engine->state = INIT_STATE_WAIT_FOR_KEY;
							break;
						}
					}
				}

				/* Iterate save process in batches */
				if (engine->save_batch_size < 1)
				{
					engine->save_batch_size = 1;
				}
				int save_result = 1;
				int iterations = 0;
				while ((save_result == 1) && (iterations < engine->save_batch_size))
				{
					save_result = modland_com_filedb_save_iterate();
					iterations++;
				}

				if (save_result == 0)
				{
					snprintf(engine->save_message, sizeof(engine->save_message), "Completed successfully");
					engine->save_complete = 1;
					engine->state = INIT_STATE_WAIT_FOR_KEY;

					/* Trigger immediate IDBFS sync after successful modland database save */
#ifdef WASM_BUILD
					extern int idbfs_sync_data_immediate(void);
					idbfs_sync_data_immediate();
#endif
				} else if (save_result < 0)
				{
					snprintf(engine->save_message, sizeof(engine->save_message), "Writing data failed");
					engine->save_complete = 2;
					engine->state = INIT_STATE_WAIT_FOR_KEY;
				}
				break;
			}

			case INIT_STATE_WAIT_FOR_KEY:
			{
				API->console->FrameLock();
				API->fsDraw();
				modland_com_initialize_Draw(
					API->console,
					2, NULL, engine->download_size, engine->year, engine->month, engine->day,
					2, NULL, modland_com.database.fileentries_n, modland_com.database.direntries_n, engine->parse_state.invalid_entries,
					engine->save_complete + 1, engine->save_message,
					modland_com.database.fileentries_n, modland_com.database.fileentries_n, engine->spinner_counter,
					0, 2
				);

				while (API->console->KeyboardHit())
				{
					int key = API->console->KeyboardGetChar();
					if (key == KEY_EXIT || key == KEY_ESC || key == _KEY_ENTER)
					{
						modland_com_initialize_engine_finish(engine);
						if (result) *result = 1;
						return;
					}
				}
				break;
			}

			case INIT_STATE_ERROR:
			{
				API->console->FrameLock();
				API->fsDraw();
				modland_com_initialize_Draw_Until_Enter_Or_Exit(
					API,
					3, engine->error_message, 0, 0, 0, 0,
					0, NULL, 0, 0, 0,
					0, NULL, 0, 0
				);
				modland_com_initialize_engine_finish(engine);
				if (result) *result = -1;
				return;
			}

			case INIT_STATE_DONE:
				modland_com_initialize_engine_finish(engine);
				if (result) *result = 1;
				return;

			default:
				engine->state = INIT_STATE_IDLE;
				break;
		}

		/* Only count this as an iteration if we did actual work.
		 * Quick state transitions (like SPAWN_DOWNLOAD->DOWNLOADING or DOWNLOADING->OPEN_ZIP)
		 * should not consume iterations, allowing the next state to run immediately.
		 */
		if (old_state == engine->state ||
		    engine->state == INIT_STATE_DONE ||
		    engine->state == INIT_STATE_ERROR ||
		    engine->state == INIT_STATE_WAIT_FOR_KEY)
		{
			iter++;
		}
	}

	/* Still in progress */
	if (result) *result = 2;
}

/* Public stepper API */
void modland_com_initialize_StepperReset(struct modland_com_initialize_engine *engine)
{
	modland_com_initialize_engine_reset(engine);
}

int modland_com_initialize_StepperIsActive(const struct modland_com_initialize_engine *engine)
{
	return engine && engine->is_active;
}

enum modland_com_initialize_state modland_com_initialize_StepperGetState(const struct modland_com_initialize_engine *engine)
{
	if (!engine) {
		return INIT_STATE_IDLE;
	}
	return engine->state;
}

int modland_com_initialize_StepperShouldRunFast(const struct modland_com_initialize_engine *engine)
{
	if (!engine || !engine->is_active) {
		return 0;
	}

	/* Run unlimited iterations during CPU-intensive states */
	switch (engine->state) {
		case INIT_STATE_PARSING:
		case INIT_STATE_SORTING:
		case INIT_STATE_SAVING:
			return 1;

		/* Single iteration during I/O waits and user interaction */
		case INIT_STATE_DOWNLOADING:
		case INIT_STATE_WAIT_FOR_KEY:
		case INIT_STATE_ERROR:
		case INIT_STATE_IDLE:
		case INIT_STATE_SPAWN_DOWNLOAD:
		case INIT_STATE_OPEN_ZIP:
		case INIT_STATE_DONE:
		default:
			return 0;
	}
}

void modland_com_initialize_StepperRun(
	struct modland_com_initialize_engine *engine,
	void **token,
	const struct DevInterfaceAPI_t *API,
	int iteration_limit,
	int *result
)
{
	modland_com_initialize_Run_impl(engine, token, API, iteration_limit, result);
}
