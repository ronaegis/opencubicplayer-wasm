/* devp-dialog-stepper.c - WASM stepper for devp setup dialog
 *
 * PURPOSE:
 *   Converts the blocking while(1) loop in dev/deviplay.c:setup_devp_run()
 *   into a non-blocking stepper using the generic dialog framework.
 *
 * USAGE:
 *   This file is conditionally included by deviplay.c when OCP_WASM_DIALOG_STEPPER
 *   is defined. It provides wasm_setup_devp_run() which wraps the original dialog
 *   logic in the generic stepper framework.
 *
 * Last updated: 2025-10-24
 */

/* Dialog state - all variables from the original while(1) loop */
struct devp_dialog_state {
	int dsel;  /* Selected driver index */
};

/* Initialize dialog state */
static void devp_dialog_init(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	struct devp_dialog_state *s = (struct devp_dialog_state *)state;
	s->dsel = 0;
	(void)token; /* Unused */
	(void)API;   /* Unused */
}

/* Draw the dialog */
static void devp_dialog_draw(void *state, const struct DevInterfaceAPI_t *API)
{
	struct devp_dialog_state *s = (struct devp_dialog_state *)state;
	setup_devp_draw_driver(API, "Playback plugins", s->dsel);
}

/* Handle keyboard input */
static int devp_dialog_handle_key(void *state, int key, const struct DevInterfaceAPI_t *API)
{
	struct devp_dialog_state *s = (struct devp_dialog_state *)state;

	switch (key)
	{
		case KEY_DOWN:
			if (s->dsel + 1 < plrDriverListEntries)
			{
				s->dsel++;
			}
			break;

		case KEY_UP:
			if (s->dsel > 0)
			{
				s->dsel--;
			}
			break;

		case KEY_HOME:
			s->dsel = 0;
			break;

		case KEY_END:
			s->dsel = plrDriverListEntries ? plrDriverListEntries - 1 : 0;
			break;

		case KEY_EXIT:
		case KEY_ESC:
			devp_save_devices(API);
			API->configAPI->StoreConfig();
			return 1; /* Exit dialog */

		case _KEY_ENTER:
			if ((s->dsel < plrDriverListEntries) &&
			    plrDriverList[s->dsel].driver &&
			    (plrDriverList[s->dsel].driver != plrDriver) &&
			    (!plrDriverList[s->dsel].disabled) &&
			    (!(plrDriverList[s->dsel].probed && !plrDriverList[s->dsel].detected)))
			{
				API->console->Driver->consoleRestore();
				if (plrDriver)
				{
					plrDriver->Close(plrDriver);
					plrDriver = 0;
				}
				if (!plrDriverList[s->dsel].probed)
				{
					plrDriverList[s->dsel].detected = plrDriverList[s->dsel].driver->Detect(plrDriverList[s->dsel].driver);
					plrDriverList[s->dsel].probed = 1;
				}
				if (plrDriverList[s->dsel].detected)
				{
					plrDevAPI = plrDriverList[s->dsel].driver->Open(plrDriverList[s->dsel].driver, &plrDriverAPI);
					if (plrDevAPI)
					{
						plrDriver = plrDriverList[s->dsel].driver;
					}
				}
				API->console->Driver->consoleSave();
			}
			break;

		case '+':
			if (s->dsel)
			{
				struct plrDriverListEntry_t temp;
				temp                       = plrDriverList[s->dsel - 1];
				plrDriverList[s->dsel - 1] = plrDriverList[s->dsel];
				plrDriverList[s->dsel]     = temp;
				s->dsel--;
			}
			break;

		case '-':
			if ((plrDriverListEntries >= 2) &&
			    (s->dsel < (plrDriverListEntries - 1)))
			{
				struct plrDriverListEntry_t temp;
				temp                       = plrDriverList[s->dsel + 1];
				plrDriverList[s->dsel + 1] = plrDriverList[s->dsel];
				plrDriverList[s->dsel]     = temp;
				s->dsel++;
			}
			break;

		case 'd':
		case 'D':
			if ((s->dsel < plrDriverListEntries) &&
			    (!(plrDriverList[s->dsel].driver && (plrDriverList[s->dsel].driver == plrDriver))) &&
			    (!plrDriverList[s->dsel].disabled) &&
			    (!(plrDriverList[s->dsel].probed && !plrDriverList[s->dsel].detected)))
			{
				plrDriverList[s->dsel].disabled = 1;
			}
			break;

		case 'e':
		case 'E':
			if ((s->dsel < plrDriverListEntries) &&
			    plrDriverList[s->dsel].disabled)
			{
				plrDriverList[s->dsel].disabled = 0;
			}
			break;

		case KEY_DELETE:
			if ((s->dsel < plrDriverListEntries) &&
			    !plrDriverList[s->dsel].driver)
			{
				/* Remove this entry from the list */
				memmove(plrDriverList + s->dsel,
				        plrDriverList + s->dsel + 1,
				        sizeof(plrDriverList[0]) * (plrDriverListEntries - s->dsel - 1));
				plrDriverListEntries--;
				if (s->dsel >= plrDriverListEntries && s->dsel > 0)
				{
					s->dsel--;
				}
			}
			break;
	}

	return 0; /* Continue dialog */
}

/* Cleanup dialog */
static void devp_dialog_cleanup(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	(void)state; /* Unused */
	(void)token; /* Unused */
	(void)API;   /* Unused */
	/* Cleanup already done in handle_key for KEY_ESC */
}

/* Dialog operations structure */
static const struct dialog_ops_t devp_dialog_ops = {
	.name = "devp_setup",
	.init = devp_dialog_init,
	.draw = devp_dialog_draw,
	.handle_key = devp_dialog_handle_key,
	.cleanup = devp_dialog_cleanup
};

/* External functions for signaling virtual device stepper state
 * These are defined in wasm/pfilesel-stepper.c and tell the
 * VirtualInterface wrapper to keep the dialog alive */
extern int wasm_get_virtual_device_stepper_active(void);
extern void wasm_set_virtual_device_stepper_active(int value);

/* WASM-compatible run function
 * Called repeatedly from the browser event loop */
static void wasm_setup_devp_run(void **token, const struct DevInterfaceAPI_t *API)
{
	static struct generic_dialog_engine engine;
	static struct devp_dialog_state state;
	static int initialized = 0;

	if (!initialized)
	{
		generic_dialog_init(&engine, &devp_dialog_ops, &state, sizeof(state));
		initialized = 1;
		/* Signal that a virtual device dialog is now active */
		wasm_set_virtual_device_stepper_active(1);
	}

	/* Run one frame iteration */
	int result = generic_dialog_run(&engine, token, API, 1);

	/* If dialog exited, reset for next time */
	if (result == 0)
	{
		initialized = 0;
		/* Signal that the virtual device dialog is now closed */
		wasm_set_virtual_device_stepper_active(0);
	}
}
