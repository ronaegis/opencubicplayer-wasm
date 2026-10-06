/* alsa-dialog-stepper.c - WASM stepper for ALSA config dialog
 *
 * PURPOSE:
 *   Converts the blocking while(1) loop in devp/devpalsa.c:alsaSetupRun()
 *   into a non-blocking stepper using the generic dialog framework.
 *
 * USAGE:
 *   This file is conditionally included by devpalsa.c when OCP_WASM_DIALOG_STEPPER
 *   is defined.
 *
 * Last updated: 2025-10-24
 */

/* Dialog state - all variables from the original while(1) loop */
struct alsa_dialog_state {
	enum alsaConfigDraw_Mode_t mode;
	struct AlsaConfigDeviceList_t pcmlist;
	struct AlsaConfigDeviceList_t mixerlist;
	int initialized;
};

/* Initialize dialog state */
static void alsa_dialog_init(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	struct alsa_dialog_state *s = (struct alsa_dialog_state *)state;

	s->mode = ACDM_AUDIO_DEVICE_SELECTED;
	memset(&s->pcmlist, 0, sizeof(s->pcmlist));
	memset(&s->mixerlist, 0, sizeof(s->mixerlist));
	s->initialized = 0;

	snprintf(s->pcmlist.custom, sizeof(s->pcmlist.custom), "%s", alsaCardName);
	snprintf(s->mixerlist.custom, sizeof(s->mixerlist.custom), "%s", alsaMixerName);

	alsaSetupScan(&s->pcmlist, &s->mixerlist);
	s->initialized = 1;

	(void)token; /* Unused */
	(void)API;   /* Unused */
}

/* Draw the dialog */
static void alsa_dialog_draw(void *state, const struct DevInterfaceAPI_t *API)
{
	struct alsa_dialog_state *s = (struct alsa_dialog_state *)state;
	const int mlWidth = 78;
	const int mlHeight = 18;
	int mlTop = (API->console->TextHeight - mlHeight) / 2;
	int mlLeft = (API->console->TextWidth - mlWidth) / 2;

	alsaSetupDraw(mlTop, mlLeft, mlHeight, mlWidth, &s->mode, &s->pcmlist, &s->mixerlist, API);
}

/* Handle keyboard input */
static int alsa_dialog_handle_key(void *state, int key, const struct DevInterfaceAPI_t *API)
{
	struct alsa_dialog_state *s = (struct alsa_dialog_state *)state;

	/* EditStringUTF8z is handled in draw, skip key processing when editing */
	if ((s->mode == ACDM_AUDIO_DEVICE_CUSTOM_EDIT) ||
	    (s->mode == ACDM_MIXER_DEVICE_CUSTOM_EDIT))
	{
		return 0; /* Continue - edit mode active */
	}

	switch (key)
	{
		case KEY_DOWN:
			switch (s->mode)
			{
				case ACDM_AUDIO_DEVICE_SELECTED:
					if (s->pcmlist.selected)
					{
						s->mode = ACDM_MIXER_DEVICE_SELECTED;
					} else {
						s->mode = ACDM_AUDIO_DEVICE_CUSTOM_SELECTED;
					}
					break;
				case ACDM_AUDIO_DEVICE_OPEN:
					if (s->pcmlist.preselected + 1 < s->pcmlist.fill)
					{
						s->pcmlist.preselected++;
					}
					break;
				case ACDM_AUDIO_DEVICE_CUSTOM_SELECTED:
					s->mode = ACDM_MIXER_DEVICE_SELECTED;
					break;
				case ACDM_MIXER_DEVICE_SELECTED:
					if (!s->mixerlist.selected)
					{
						s->mode = ACDM_MIXER_DEVICE_CUSTOM_SELECTED;
					}
					break;
				case ACDM_MIXER_DEVICE_OPEN:
					if (s->mixerlist.preselected + 1 < s->mixerlist.fill)
					{
						s->mixerlist.preselected++;
					}
					break;
				default:
					break;
			}
			break;

		case KEY_UP:
			switch (s->mode)
			{
				case ACDM_AUDIO_DEVICE_OPEN:
					if (s->pcmlist.preselected > 0)
					{
						s->pcmlist.preselected--;
					}
					break;
				case ACDM_AUDIO_DEVICE_CUSTOM_SELECTED:
					s->mode = ACDM_AUDIO_DEVICE_SELECTED;
					break;
				case ACDM_MIXER_DEVICE_SELECTED:
					if (s->pcmlist.selected)
					{
						s->mode = ACDM_AUDIO_DEVICE_SELECTED;
					} else {
						s->mode = ACDM_AUDIO_DEVICE_CUSTOM_SELECTED;
					}
					break;
				case ACDM_MIXER_DEVICE_OPEN:
					if (s->mixerlist.preselected > 0)
					{
						s->mixerlist.preselected--;
					}
					break;
				case ACDM_MIXER_DEVICE_CUSTOM_SELECTED:
					s->mode = ACDM_MIXER_DEVICE_SELECTED;
					break;
				default:
					break;
			}
			break;

		case KEY_EXIT:
			return 1; /* Exit dialog */

		case KEY_ESC:
			switch (s->mode)
			{
				case ACDM_AUDIO_DEVICE_SELECTED:
				case ACDM_AUDIO_DEVICE_CUSTOM_SELECTED:
				case ACDM_MIXER_DEVICE_SELECTED:
				case ACDM_MIXER_DEVICE_CUSTOM_SELECTED:
					return 1; /* Exit dialog */
				case ACDM_AUDIO_DEVICE_OPEN:
					s->mode = ACDM_AUDIO_DEVICE_SELECTED;
					break;
				case ACDM_AUDIO_DEVICE_CUSTOM_EDIT:
					s->mode = ACDM_AUDIO_DEVICE_CUSTOM_SELECTED;
					API->console->Driver->SetCursorShape(0);
					break;
				case ACDM_MIXER_DEVICE_OPEN:
					s->mode = ACDM_MIXER_DEVICE_SELECTED;
					break;
				case ACDM_MIXER_DEVICE_CUSTOM_EDIT:
					s->mode = ACDM_MIXER_DEVICE_CUSTOM_SELECTED;
					API->console->Driver->SetCursorShape(0);
					break;
			}
			break;

		case _KEY_ENTER:
			switch (s->mode)
			{
				case ACDM_AUDIO_DEVICE_SELECTED:
					s->mode = ACDM_AUDIO_DEVICE_OPEN;
					s->pcmlist.preselected = s->pcmlist.selected;
					break;
				case ACDM_AUDIO_DEVICE_OPEN:
					s->mode = ACDM_AUDIO_DEVICE_SELECTED;
					s->pcmlist.selected = s->pcmlist.preselected;
					break;
				case ACDM_AUDIO_DEVICE_CUSTOM_SELECTED:
					s->mode = ACDM_AUDIO_DEVICE_CUSTOM_EDIT;
					break;
				case ACDM_MIXER_DEVICE_SELECTED:
					s->mode = ACDM_MIXER_DEVICE_OPEN;
					s->mixerlist.preselected = s->mixerlist.selected;
					break;
				case ACDM_MIXER_DEVICE_OPEN:
					s->mode = ACDM_MIXER_DEVICE_SELECTED;
					s->mixerlist.selected = s->mixerlist.preselected;
					break;
				case ACDM_MIXER_DEVICE_CUSTOM_SELECTED:
					s->mode = ACDM_MIXER_DEVICE_CUSTOM_EDIT;
					break;
				default:
					break;
			}
			break;
	}

	return 0; /* Continue dialog */
}

/* Cleanup dialog */
static void alsa_dialog_cleanup(void *state, void **token, const struct DevInterfaceAPI_t *API)
{
	struct alsa_dialog_state *s = (struct alsa_dialog_state *)state;

	if (s->initialized)
	{
		/* Save selected devices */
		if (s->pcmlist.selected)
		{
			snprintf(alsaCardName, sizeof(alsaCardName), "%.*s",
			         (unsigned int)sizeof(alsaCardName) - 1,
			         s->pcmlist.entries[s->pcmlist.selected].name);
		} else {
			snprintf(alsaCardName, sizeof(alsaCardName), "%.*s",
			         (unsigned int)sizeof(alsaCardName) - 1,
			         s->pcmlist.custom);
		}

		if (s->mixerlist.selected)
		{
			snprintf(alsaMixerName, sizeof(alsaMixerName), "%.*s",
			         (unsigned int)sizeof(alsaMixerName) - 1,
			         s->mixerlist.entries[s->mixerlist.selected].name);
		} else {
			snprintf(alsaMixerName, sizeof(alsaMixerName), "%.*s",
			         (unsigned int)sizeof(alsaMixerName) - 1,
			         s->mixerlist.custom);
		}

		debug_printf("ALSA: Selected PCM %s\n", alsaCardName);
		debug_printf("ALSA: Selected Mixer %s\n", alsaMixerName);

		alsaSetupClearList(&s->pcmlist);
		alsaSetupClearList(&s->mixerlist);

		API->configAPI->SetProfileString("devpALSA", "card", alsaCardName);
		API->configAPI->SetProfileString("devpALSA", "mixer", alsaMixerName);
		API->configAPI->StoreConfig();

		s->initialized = 0;
	}

	(void)token; /* Unused */
}

/* Dialog operations structure */
static const struct dialog_ops_t alsa_dialog_ops = {
	.name = "alsa_config",
	.init = alsa_dialog_init,
	.draw = alsa_dialog_draw,
	.handle_key = alsa_dialog_handle_key,
	.cleanup = alsa_dialog_cleanup
};

/* External functions for signaling virtual device stepper state */
extern int wasm_get_virtual_device_stepper_active(void);
extern void wasm_set_virtual_device_stepper_active(int value);

/* WASM-compatible run function */
static void wasm_alsaSetupRun(void **token, const struct DevInterfaceAPI_t *API)
{
	static struct generic_dialog_engine engine;
	static struct alsa_dialog_state state;
	static int initialized = 0;

	if (!initialized)
	{
		generic_dialog_init(&engine, &alsa_dialog_ops, &state, sizeof(state));
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
