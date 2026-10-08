/* timidity-dialog-stepper.c - WASM stepper for TiMidity config dialog
 * Simplified version - Nested file selector disabled in WASM for now
 */

struct timidity_dialog_state {
	int esel;
	uint32_t lastpress;
	int repeat;
};

static void timidity_dialog_init(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	struct timidity_dialog_state *s = (struct timidity_dialog_state *)state;

	refresh_configfiles(API);

	DefaultReverbMode     = API->configAPI->GetProfileInt("timidity", "reverbmode",       2, 10);
	DefaultReverbLevel    = API->configAPI->GetProfileInt("timidity", "reverblevel",     40, 10);
	DefaultScaleRoom      = API->configAPI->GetProfileInt("timidity", "scaleroom",       28, 10);
	DefaultOffsetRoom     = API->configAPI->GetProfileInt("timidity", "offsetroom",      70, 10);
	DefaultPredelayFactor = API->configAPI->GetProfileInt("timidity", "predelayfactor", 100, 10);
	DefaultDelayMode      = API->configAPI->GetProfileInt("timidity", "delaymode",       -1, 10) + 1;
	DefaultDelay          = API->configAPI->GetProfileInt("timidity", "delay",           25, 10);
	DefaultChorus         = API->configAPI->GetProfileInt("timidity", "chorusenabled",    1, 10);

	/* Clamp values */
	if (DefaultReverbMode     <    0) DefaultReverbMode     =    0;
	if (DefaultReverbLevel    <    0) DefaultReverbLevel    =    0;
	if (DefaultScaleRoom      <    0) DefaultScaleRoom      =    0;
	if (DefaultOffsetRoom     <    0) DefaultOffsetRoom     =    0;
	if (DefaultPredelayFactor <    0) DefaultPredelayFactor =    0;
	if (DefaultDelayMode      <    0) DefaultDelayMode      =    0;
	if (DefaultDelay          <    0) DefaultDelay          =    0;
	if (DefaultChorus         <    0) DefaultChorus         =    0;
	if (DefaultReverbMode     >    4) DefaultReverbMode     =    2;
	if (DefaultReverbLevel    >  127) DefaultReverbLevel    =  127;
	if (DefaultScaleRoom      > 1000) DefaultScaleRoom      = 1000;
	if (DefaultOffsetRoom     > 1000) DefaultOffsetRoom     = 1000;
	if (DefaultPredelayFactor > 1000) DefaultPredelayFactor = 1000;
	if (DefaultDelayMode      >    3) DefaultDelayMode      =    3;
	if (DefaultDelay          > 1000) DefaultDelay          = 1000;
	if (DefaultChorus         >    1) DefaultChorus         =    1;

	s->esel = 0;
	s->lastpress = 0;
	s->repeat = 1;

	(void)token;
}

static void timidity_dialog_draw(void *state, const struct DevInterfaceAPI_t *API)
{
	struct timidity_dialog_state *s = (struct timidity_dialog_state *)state;
	timidityConfigDraw(s->esel, API);
}

static int timidity_dialog_handle_key(void *state, int key, const struct DevInterfaceAPI_t *API)
{
	struct timidity_dialog_state *s = (struct timidity_dialog_state *)state;

	/* Update repeat acceleration */
	if ((key != KEY_LEFT) && (key != KEY_RIGHT))
	{
		s->lastpress = 0;
		s->repeat = 1;
	} else {
		uint32_t newpress = clock_ms();
		if ((newpress - s->lastpress) > 250) /* 250 ms */
		{
			s->repeat = 1;
		} else {
			if (s->repeat < 20)
			{
				s->repeat += 1;
			}
		}
		s->lastpress = newpress;
	}

	switch (key)
	{
		case _KEY_ENTER:
			/* Nested file selector disabled in WASM for now */
			break;

		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			s->esel = key - '1';
			break;

		case KEY_LEFT:
			switch (s->esel)
			{
				case 1:
					if (DefaultReverbMode) DefaultReverbMode--;
					break;
				case 2:
					if (s->repeat > DefaultReverbLevel) DefaultReverbLevel = 0;
					else DefaultReverbLevel -= s->repeat;
					break;
				case 3:
					if (s->repeat > DefaultScaleRoom) DefaultScaleRoom = 0;
					else DefaultScaleRoom -= s->repeat;
					break;
				case 4:
					if (s->repeat > DefaultOffsetRoom) DefaultOffsetRoom = 0;
					else DefaultOffsetRoom -= s->repeat;
					break;
				case 5:
					if (s->repeat > DefaultPredelayFactor) DefaultPredelayFactor = 0;
					else DefaultPredelayFactor -= s->repeat;
					break;
				case 6:
					if (DefaultDelayMode) DefaultDelayMode -= 1;
					break;
				case 7:
					if ((s->repeat + 1) > DefaultDelay) DefaultDelay = 1;
					else DefaultDelay -= s->repeat;
					break;
				case 8:
					if (DefaultChorus) DefaultChorus -= 1;
					break;
			}
			break;

		case KEY_RIGHT:
			switch (s->esel)
			{
				case 1:
					if (DefaultReverbMode < 4) DefaultReverbMode++;
					break;
				case 2:
					if ((DefaultReverbLevel + s->repeat) > 127) DefaultReverbLevel = 127;
					else DefaultReverbLevel += s->repeat;
					break;
				case 3:
					if ((DefaultScaleRoom + s->repeat) > 1000) DefaultScaleRoom = 1000;
					else DefaultScaleRoom += s->repeat;
					break;
				case 4:
					if ((DefaultOffsetRoom + s->repeat) > 1000) DefaultOffsetRoom = 1000;
					else DefaultOffsetRoom += s->repeat;
					break;
				case 5:
					if ((DefaultPredelayFactor + s->repeat) > 1000) DefaultPredelayFactor = 1000;
					else DefaultPredelayFactor += s->repeat;
					break;
				case 6:
					if (DefaultDelayMode < 3) DefaultDelayMode++;
					break;
				case 7:
					if ((DefaultDelay + s->repeat) > 1000) DefaultDelay = 1000;
					else DefaultDelay += s->repeat;
					break;
				case 8:
					if (!DefaultChorus) DefaultChorus = 1;
					break;
			}
			break;

		case KEY_DOWN:
			if (s->esel < 8) s->esel++;
			break;

		case KEY_UP:
			if (s->esel) s->esel--;
			break;

		case KEY_EXIT:
		case KEY_ESC:
			return 1; /* Exit dialog. Cleanup applies the values and stores them. */
	}

	return 0; /* Continue dialog */
}

static void timidity_dialog_cleanup(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	reset_configfiles(API);

	API->configAPI->SetProfileInt("timidity", "reverbmode",     DefaultReverbMode,     10);
	API->configAPI->SetProfileInt("timidity", "reverblevel",    DefaultReverbLevel,    10);
	API->configAPI->SetProfileInt("timidity", "scaleroom",      DefaultScaleRoom,      10);
	API->configAPI->SetProfileInt("timidity", "offsetroom",     DefaultOffsetRoom,     10);
	API->configAPI->SetProfileInt("timidity", "predelayfactor", DefaultPredelayFactor, 10);
	API->configAPI->SetProfileInt("timidity", "delaymode",      DefaultDelayMode - 1,  10);
	API->configAPI->SetProfileInt("timidity", "delay",          DefaultDelay,          10);
	API->configAPI->SetProfileInt("timidity", "chorusenabled",  DefaultChorus,         10);
	API->configAPI->StoreConfig();

	(void)state;
	(void)token;
}

static const struct dialog_ops_t timidity_dialog_ops = {
	.name = "timidity_config",
	.init = timidity_dialog_init,
	.draw = timidity_dialog_draw,
	.handle_key = timidity_dialog_handle_key,
	.cleanup = timidity_dialog_cleanup
};

extern int wasm_get_virtual_device_stepper_active(void);
extern void wasm_set_virtual_device_stepper_active(int value);

static void wasm_timidityConfigRun(void **token, const struct DevInterfaceAPI_t *API)
{
	static struct generic_dialog_engine engine;
	static struct timidity_dialog_state state;
	static int initialized = 0;

	if (!initialized)
	{
		generic_dialog_init(&engine, &timidity_dialog_ops, &state, sizeof(state));
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
