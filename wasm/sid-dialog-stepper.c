/* sid-dialog-stepper.c - WASM stepper for SID config dialog
 * Simplified version - Nested file browser disabled in WASM for now
 */

struct sid_dialog_state {
	int esel;
	uint32_t lastpress;
	uint16_t lastkey;
	int repeat;
};

static void sid_dialog_init(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	struct sid_dialog_state *s = (struct sid_dialog_state *)state;
	uint32_t dirdb_base = API->configAPI->DataHomeDir->dirdb_ref;

	config_emulator = emulator_to_int         (API->configAPI->GetProfileString ("libsidplayfp", "emulator",        "residfp"));
	config_defaultC64 = defaultC64_to_int     (API->configAPI->GetProfileString ("libsidplayfp", "defaultC64",      "PAL"));
	config_forceC64 =                          API->configAPI->GetProfileBool   ("libsidplayfp", "forceC64",        0, 0);
	config_defaultSID = defaultSID_to_int     (API->configAPI->GetProfileString ("libsidplayfp", "defaultSID",      "MOS6581"));
	config_forceSID =                          API->configAPI->GetProfileBool   ("libsidplayfp", "forceSID",        0, 0);
	config_CIA = CIA_to_int                   (API->configAPI->GetProfileString ("libsidplayfp", "CIA",             "MOS6526"));
	config_filter =                            API->configAPI->GetProfileBool   ("libsidplayfp", "filter",          1, 1);
	config_filtercurve6581 = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "filtercurve6581", "0.5"));
	config_filterrange6581 = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "filterrange6581", "0.5"));
	config_waveoffset6581  = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "waveoffset6581",  "1.0"));
	config_enableOld6581caps =                 API->configAPI->GetProfileBool   ("libsidplayfp", "enableOld6581caps", 0, 0);
	config_filtercurve8580 = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "filtercurve8580", "0.5"));
	config_combinedwaveforms = CWS_to_int     (API->configAPI->GetProfileString ("libsidplayfp", "combinedwaveforms", "Strong"));
	config_digiboost =                         API->configAPI->GetProfileBool   ("libsidplayfp", "digiboost",       0, 0);
	config_dacLeakageLevel = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "dacLeakageLevel", "1.0"));
	config_dcBlockResistor = float100x_to_int (API->configAPI->GetProfileString ("libsidplayfp", "dcBlockResistor", "0.0"));
	config_kernal = strdup                    (API->configAPI->GetProfileString ("libsidplayfp", "kernal",          "KERNEL.ROM"));
	config_basic = strdup                     (API->configAPI->GetProfileString ("libsidplayfp", "basic",           "BASIC.ROM"));
	config_chargen = strdup                   (API->configAPI->GetProfileString ("libsidplayfp", "chargen",         "CHARGEN.ROM"));

	if (config_filtercurve6581 <   0) config_filtercurve6581 =   0;
	if (config_filtercurve6581 > 100) config_filtercurve6581 = 100;
	if (config_filterrange6581 <   0) config_filterrange6581 =   0;
	if (config_filterrange6581 > 100) config_filterrange6581 = 100;
	if (config_waveoffset6581  <   0) config_waveoffset6581  =   0;
	if (config_waveoffset6581  > 100) config_waveoffset6581  = 100;
	if (config_filtercurve8580 <   0) config_filtercurve8580 =   0;
	if (config_filtercurve8580 > 100) config_filtercurve8580 = 100;
	if (config_dacLeakageLevel <   0) config_dacLeakageLevel =   0;
	if (config_dacLeakageLevel > 100) config_dacLeakageLevel = 100;
	if (config_dcBlockResistor <   0) config_dcBlockResistor =   0;
	if (config_dcBlockResistor > 100) config_dcBlockResistor = 100;

	entry_kernal.dirdb_ref  = API->dirdb->ResolvePathWithBaseAndRef (dirdb_base, config_kernal,  DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
	entry_basic.dirdb_ref   = API->dirdb->ResolvePathWithBaseAndRef (dirdb_base, config_basic,   DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);
	entry_chargen.dirdb_ref = API->dirdb->ResolvePathWithBaseAndRef (dirdb_base, config_chargen, DIRDB_RESOLVE_DRIVE | DIRDB_RESOLVE_TILDE_HOME, dirdb_use_file);

	rom_md5s (entry_kernal .hash_4096, entry_kernal .hash_8192, entry_kernal .dirdb_ref, API);
	rom_md5s (entry_basic  .hash_4096, entry_basic  .hash_8192, entry_basic  .dirdb_ref, API);
	rom_md5s (entry_chargen.hash_4096, entry_chargen.hash_8192, entry_chargen.dirdb_ref, API);

	s->esel = 0;
	s->lastpress = 0;
	s->lastkey = 0;
	s->repeat = 1;

	(void)token;
}

static void sid_dialog_draw(void *state, const struct DevInterfaceAPI_t *API)
{
	struct sid_dialog_state *s = (struct sid_dialog_state *)state;
	sidConfigDraw(s->esel, API);
}

static int sid_dialog_handle_key(void *state, int key, const struct DevInterfaceAPI_t *API)
{
	struct sid_dialog_state *s = (struct sid_dialog_state *)state;

	/* Update repeat acceleration */
	if ((key != KEY_LEFT) && (key != KEY_RIGHT) && (key != s->lastkey))
	{
		s->lastkey = key;
		s->lastpress = 0;
		s->repeat = 1;
	} else {
		uint32_t newpress = clock_ms();
		if ((newpress - s->lastpress) > 250) /* 250 ms */
		{
			s->repeat = 1;
		} else {
			if (s->esel == 7)
			{
				if (s->repeat < 20) s->repeat += 1;
			} else {
				if (s->repeat < 5) s->repeat += 1;
			}
		}
		s->lastpress = newpress;
	}

	switch (key)
	{
		case _KEY_ENTER:
			/* Nested file browser disabled in WASM for now */
			break;

		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			s->esel = key - '1';
			break;

		case 'a': case 'b': case 'c': case 'd': case 'e':
		case 'f': case 'g': case 'h': case 'i': case 'j':
			s->esel = key - 'a' + 9;
			break;

		case 'A': case 'B': case 'C': case 'D': case 'E':
		case 'F': case 'G': case 'H': case 'I': case 'J':
			s->esel = key - 'A' + 9;
			break;

		case KEY_LEFT:
			switch (s->esel)
			{
				case SEL_EMULATOR:        config_emulator = 0; break;
				case SEL_DEFAULTC64MODEL: config_defaultC64 -= (!!config_defaultC64); break;
				case SEL_FORCEC64MODEL:   config_forceC64 = 0; break;
				case SEL_DEFAULTSID:      config_defaultSID -= (!!config_defaultSID); break;
				case SEL_FORCESID:        config_forceSID = 0; break;
				case SEL_CIAMODEL:        config_CIA -= (!!config_CIA); break;
				case SEL_FILTER:          config_filter = 0; break;
				case SEL_FILTERCURVE6581:
					config_filtercurve6581 -= s->repeat;
					if (config_filtercurve6581 < 0) config_filtercurve6581 = 0;
					break;
				case SEL_FILTERRANGE6581:
					config_filterrange6581 -= s->repeat;
					if (config_filterrange6581 < 0) config_filterrange6581 = 0;
					break;
				case SEL_WAVEOFFSET6581:
					config_waveoffset6581 -= s->repeat;
					if (config_waveoffset6581 < 0) config_waveoffset6581 = 0;
					break;
				case SEL_ENABLEOLD6581CAPS:
					config_enableOld6581caps = 0;
					break;
				case SEL_FILTERCURVE8580:
					config_filtercurve8580 -= s->repeat;
					if (config_filtercurve8580 < 0) config_filtercurve8580 = 0;
					break;
				case SEL_CWS:
					config_combinedwaveforms -= 1;
					if (config_combinedwaveforms < 0) config_combinedwaveforms = 0;
					break;
				case SEL_DIGIBOOST:       config_digiboost = 0; break;
				case SEL_DACLEAKAGELEVEL:
					config_dacLeakageLevel -= s->repeat;
					if (config_dacLeakageLevel < 0) config_dacLeakageLevel = 0;
					break;
				case SEL_DCBLOCKRESISTANCE:
					config_dcBlockResistor -= s->repeat;
					if (config_dcBlockResistor < 0) config_dcBlockResistor = 0;
					break;
				default:
					break;
			}
			break;

		case KEY_RIGHT:
			switch (s->esel)
			{
				case SEL_EMULATOR:        config_emulator = 1; break;
				case SEL_DEFAULTC64MODEL: config_defaultC64 += (config_defaultC64 != 4); break;
				case SEL_FORCEC64MODEL:   config_forceC64 = 1; break;
				case SEL_DEFAULTSID:      config_defaultSID = 1; break;
				case SEL_FORCESID:        config_forceSID = 1; break;
				case SEL_CIAMODEL:        config_CIA += (config_CIA != 2); break;
				case SEL_FILTER:          config_filter = 1; break;
				case SEL_FILTERCURVE6581:
					config_filtercurve6581 += s->repeat;
					if (config_filtercurve6581 > 100) config_filtercurve6581 = 100;
					break;
				case SEL_FILTERRANGE6581:
					config_filterrange6581 += s->repeat;
					if (config_filterrange6581 > 100) config_filterrange6581 = 100;
					break;
				case SEL_WAVEOFFSET6581:
					config_waveoffset6581 += s->repeat;
					if (config_waveoffset6581 > 100) config_waveoffset6581 = 100;
					break;
				case SEL_ENABLEOLD6581CAPS:
					config_enableOld6581caps = 1;
					break;
				case SEL_FILTERCURVE8580:
					config_filtercurve8580 += s->repeat;
					if (config_filtercurve8580 > 100) config_filtercurve8580 = 100;
					break;
				case SEL_CWS:
					config_combinedwaveforms += 1;
					if (config_combinedwaveforms > 2) config_combinedwaveforms = 2;
					break;
				case SEL_DIGIBOOST:       config_digiboost = 1; break;
				case SEL_DACLEAKAGELEVEL:
					config_dacLeakageLevel += s->repeat;
					if (config_dacLeakageLevel > 100) config_dacLeakageLevel = 100;
					break;
				case SEL_DCBLOCKRESISTANCE:
					config_dcBlockResistor += s->repeat;
					if (config_dcBlockResistor > 100) config_dcBlockResistor = 100;
					break;
				default:
					break;
			}
			break;

		case KEY_DOWN:
			if (s->esel < SEL_ROM_CHARGEN) s->esel++;
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

static void sid_dialog_cleanup(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	API->configAPI->SetProfileString ("libsidplayfp", "emulator", emulator_from_int (config_emulator));
	API->configAPI->SetProfileString ("libsidplayfp", "defaultC64", defaultC64_from_int (config_defaultC64));
	API->configAPI->SetProfileBool   ("libsidplayfp", "forceC64", config_forceC64);
	API->configAPI->SetProfileString ("libsidplayfp", "defaultSID", defaultSID_from_int (config_defaultSID));
	API->configAPI->SetProfileBool   ("libsidplayfp", "forceSID", config_forceSID);
	API->configAPI->SetProfileString ("libsidplayfp", "CIA", CIA_from_int (config_CIA));
	API->configAPI->SetProfileBool   ("libsidplayfp", "filter", config_filter);
	API->configAPI->SetProfileString ("libsidplayfp", "filtercurve6581", int_to_float100x(config_filtercurve6581));
	API->configAPI->SetProfileString ("libsidplayfp", "filterrange6581", int_to_float100x(config_filterrange6581));
	API->configAPI->SetProfileString ("libsidplayfp", "waveoffset6581", int_to_float100x(config_waveoffset6581));
	API->configAPI->SetProfileBool   ("libsidplayfp", "enableOld6581caps", config_enableOld6581caps);
	API->configAPI->SetProfileString ("libsidplayfp", "filtercurve8580", int_to_float100x(config_filtercurve8580));
	API->configAPI->SetProfileString ("libsidplayfp", "combinedwaveforms", CWS_from_int (config_combinedwaveforms));
	API->configAPI->SetProfileBool   ("libsidplayfp", "digiboost", config_digiboost);
	API->configAPI->SetProfileString ("libsidplayfp", "dacLeakageLevel", int_to_float100x(config_dacLeakageLevel));
	API->configAPI->SetProfileString ("libsidplayfp", "dcBlockResistor", int_to_float100x(config_dcBlockResistor));
	API->configAPI->SetProfileString ("libsidplayfp", "kernal",    config_kernal); free (config_kernal);
	API->configAPI->SetProfileString ("libsidplayfp", "basic",     config_basic); free (config_basic);
	API->configAPI->SetProfileString ("libsidplayfp", "chargen",   config_chargen); free (config_chargen);
	API->configAPI->StoreConfig ();

	API->dirdb->Unref (entry_kernal.dirdb_ref, dirdb_use_file);
	API->dirdb->Unref (entry_basic.dirdb_ref, dirdb_use_file);
	API->dirdb->Unref (entry_chargen.dirdb_ref, dirdb_use_file);

	(void)state;
	(void)token;
}

static const struct dialog_ops_t sid_dialog_ops = {
	.name = "sid_config",
	.init = sid_dialog_init,
	.draw = sid_dialog_draw,
	.handle_key = sid_dialog_handle_key,
	.cleanup = sid_dialog_cleanup
};

extern int wasm_get_virtual_device_stepper_active(void);
extern void wasm_set_virtual_device_stepper_active(int value);

static void wasm_sidConfigRun(void **token, const struct DevInterfaceAPI_t *API)
{
	static struct generic_dialog_engine engine;
	static struct sid_dialog_state state;
	static int initialized = 0;

	if (!initialized)
	{
		generic_dialog_init(&engine, &sid_dialog_ops, &state, sizeof(state));
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
