/* opl-dialog-stepper.cpp - WASM stepper for OPL config dialog
 * Simplified version - RetroWave nested config disabled in WASM for now
 */

struct opl_dialog_state {
	int esel;
};

static void opl_dialog_init(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	struct opl_dialog_state *s = (struct opl_dialog_state *)state;
	const char *str = API->configAPI->GetProfileString("adplug", "emulator", "nuked");
	
	if (!strcasecmp(str, "ken")) s->esel = 0;
	else if (!strcasecmp(str, "satoh")) s->esel = 2;
	else if (!strcasecmp(str, "woody")) s->esel = 3;
	else if (!strcasecmp(str, "retrowave")) s->esel = 4;
	else s->esel = 1; /* nuked */
	
	(void)token;
}

static void opl_dialog_draw(void *state, const struct DevInterfaceAPI_t *API)
{
	struct opl_dialog_state *s = (struct opl_dialog_state *)state;
	oplConfigDraw(s->esel, API);
}

static int opl_dialog_handle_key(void *state, int key, const struct DevInterfaceAPI_t *API)
{
	struct opl_dialog_state *s = (struct opl_dialog_state *)state;

	switch (key)
	{
		case '1': case '2': case '3': case '4': 
			s->esel = key - '1'; 
			break;
		case '5':
			/* RetroWave config - disabled in WASM for now */
			break;
		case KEY_DOWN:
			if (s->esel < 3) s->esel++; /* Skip RetroWave (4) in WASM */
			break;
		case KEY_UP:
			if (s->esel) s->esel--;
			break;
		case _KEY_ENTER:
			/* RetroWave nested config disabled in WASM */
			break;
		case KEY_EXIT:
		case KEY_ESC:
			return 1;
	}
	return 0;
}

static void opl_dialog_cleanup(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	struct opl_dialog_state *s = (struct opl_dialog_state *)state;
	
	if (s->esel == 0) API->configAPI->SetProfileString("adplug", "emulator", "ken");
	else if (s->esel == 1) API->configAPI->SetProfileString("adplug", "emulator", "nuked");
	else if (s->esel == 2) API->configAPI->SetProfileString("adplug", "emulator", "satoh");
	else API->configAPI->SetProfileString("adplug", "emulator", "woody");
	API->configAPI->StoreConfig();
	
	(void)token;
}

static const struct dialog_ops_t opl_dialog_ops = {
	.name = "opl_config",
	.init = opl_dialog_init,
	.draw = opl_dialog_draw,
	.handle_key = opl_dialog_handle_key,
	.cleanup = opl_dialog_cleanup
};

extern "C" int wasm_get_virtual_device_stepper_active(void);
extern "C" void wasm_set_virtual_device_stepper_active(int value);

static void wasm_oplConfigRun(void **token, const struct DevInterfaceAPI_t *API)
{
	static struct generic_dialog_engine engine;
	static struct opl_dialog_state state;
	static int initialized = 0;

	if (!initialized)
	{
		generic_dialog_init(&engine, &opl_dialog_ops, &state, sizeof(state));
		initialized = 1;
		wasm_set_virtual_device_stepper_active(1);
	}

	int result = generic_dialog_run(&engine, token, API, 1);

	if (result == 0)
	{
		initialized = 0;
		wasm_set_virtual_device_stepper_active(0);
	}
}
