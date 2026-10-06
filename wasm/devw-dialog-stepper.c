/* devw-dialog-stepper.c - WASM stepper for devw setup dialog
 *
 * PURPOSE:
 *   Converts the blocking while(1) loop in dev/deviwave.c:setup_devw_run()
 *   into a non-blocking stepper using the generic dialog framework.
 *
 * USAGE:
 *   This file is conditionally included by deviwave.c when OCP_WASM_DIALOG_STEPPER
 *   is defined. It provides wasm_setup_devw_run() which wraps the original dialog
 *   logic in the generic stepper framework.
 *
 * Last updated: 2025-10-24
 */

/* Dialog state - all variables from the original while(1) loop */
struct devw_dialog_state {
	int dsel;  /* Selected driver index */
};

/* Initialize dialog state */
static void devw_dialog_init(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	struct devw_dialog_state *s = (struct devw_dialog_state *)state;
	s->dsel = 0;
	(void)token; /* Unused */
	(void)API;   /* Unused */
}

/* Draw the dialog */
static void devw_dialog_draw(void *state, const struct DevInterfaceAPI_t *API)
{
	struct devw_dialog_state *s = (struct devw_dialog_state *)state;
	setup_devw_draw(API, "Wavetable plugins", s->dsel);
}

/* Handle keyboard input */
static int devw_dialog_handle_key(void *state, int key, const struct DevInterfaceAPI_t *API)
{
	struct devw_dialog_state *s = (struct devw_dialog_state *)state;

	switch (key)
	{
		case KEY_DOWN:
			if (s->dsel + 1 < mcpDriverListEntries)
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
			s->dsel = mcpDriverListEntries ? mcpDriverListEntries - 1 : 0;
			break;

		case KEY_EXIT:
		case KEY_ESC:
			devw_save_devices(API);
			API->configAPI->StoreConfig();
			return 1; /* Exit dialog */

		case _KEY_ENTER:
			if ((s->dsel < mcpDriverListEntries) &&
			    mcpDriverList[s->dsel].driver &&
			    (mcpDriverList[s->dsel].driver != mcpDriver) &&
			    (!mcpDriverList[s->dsel].disabled) &&
			    (!(mcpDriverList[s->dsel].probed && !mcpDriverList[s->dsel].detected)))
			{
				API->console->Driver->consoleRestore();
				if (mcpDriver)
				{
					mcpDriver->Close(mcpDriver);
					mcpDriver = 0;
				}
				if (!mcpDriverList[s->dsel].probed)
				{
					mcpDriverList[s->dsel].detected = mcpDriverList[s->dsel].driver->Detect(mcpDriverList[s->dsel].driver);
					mcpDriverList[s->dsel].probed = 1;
				}
				if (mcpDriverList[s->dsel].detected)
				{
					mcpDevAPI = mcpDriverList[s->dsel].driver->Open(mcpDriverList[s->dsel].driver, API->configAPI, mixAPI);
					if (mcpDevAPI)
					{
						mcpDriver = mcpDriverList[s->dsel].driver;
					}
				}
				API->console->Driver->consoleSave();
			}
			break;

		case '+':
			if (s->dsel)
			{
				struct mcpDriverListEntry_t temp;
				temp                       = mcpDriverList[s->dsel - 1];
				mcpDriverList[s->dsel - 1] = mcpDriverList[s->dsel];
				mcpDriverList[s->dsel]     = temp;
				s->dsel--;
			}
			break;

		case '-':
			if ((mcpDriverListEntries >= 2) &&
			    (s->dsel < (mcpDriverListEntries - 1)))
			{
				struct mcpDriverListEntry_t temp;
				temp                       = mcpDriverList[s->dsel + 1];
				mcpDriverList[s->dsel + 1] = mcpDriverList[s->dsel];
				mcpDriverList[s->dsel]     = temp;
				s->dsel++;
			}
			break;

		case 'd':
		case 'D':
			if ((s->dsel < mcpDriverListEntries) &&
			    (!(mcpDriverList[s->dsel].driver && (mcpDriverList[s->dsel].driver == mcpDriver))) &&
			    (!mcpDriverList[s->dsel].disabled) &&
			    (!(mcpDriverList[s->dsel].probed && !mcpDriverList[s->dsel].detected)))
			{
				mcpDriverList[s->dsel].disabled = 1;
			}
			break;

		case 'e':
		case 'E':
			if ((s->dsel < mcpDriverListEntries) &&
			    mcpDriverList[s->dsel].disabled)
			{
				mcpDriverList[s->dsel].disabled = 0;
			}
			break;

		case KEY_DELETE:
			if ((s->dsel < mcpDriverListEntries) &&
			    !mcpDriverList[s->dsel].driver)
			{
				/* Remove this entry from the list */
				memmove(mcpDriverList + s->dsel,
				        mcpDriverList + s->dsel + 1,
				        sizeof(mcpDriverList[0]) * (mcpDriverListEntries - s->dsel - 1));
				mcpDriverListEntries--;
				if (s->dsel >= mcpDriverListEntries && s->dsel > 0)
				{
					s->dsel--;
				}
			}
			break;
	}

	return 0; /* Continue dialog */
}

/* Cleanup dialog */
static void devw_dialog_cleanup(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	(void)state; /* Unused */
	(void)token; /* Unused */
	(void)API;   /* Unused */
	/* Cleanup already done in handle_key for KEY_ESC */
}

/* Dialog operations structure */
static const struct dialog_ops_t devw_dialog_ops = {
	.name = "devw_setup",
	.init = devw_dialog_init,
	.draw = devw_dialog_draw,
	.handle_key = devw_dialog_handle_key,
	.cleanup = devw_dialog_cleanup
};

/* External functions for signaling virtual device stepper state
 * These are defined in wasm/pfilesel-stepper.c and tell the
 * VirtualInterface wrapper to keep the dialog alive */
extern int wasm_get_virtual_device_stepper_active(void);
extern void wasm_set_virtual_device_stepper_active(int value);

/* WASM-compatible run function
 * Called repeatedly from the browser event loop */
static void wasm_setup_devw_run(void **token, const struct DevInterfaceAPI_t *API)
{
	static struct generic_dialog_engine engine;
	static struct devw_dialog_state state;
	static int initialized = 0;

	if (!initialized)
	{
		generic_dialog_init(&engine, &devw_dialog_ops, &state, sizeof(state));
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
