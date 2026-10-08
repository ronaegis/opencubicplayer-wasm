#include "config.h"
#include "../boot/console.h"
#include "../stuff/poutput.h"
#include "../stuff/framelock.h"
#include "../help/cphelper.h"
#include "fsHelp-stepper.h"

static struct fsHelp_stepper_engine g_fsHelp_engine;

static int fsHelp_stepper_leave(uint16_t key)
{
	switch (key)
	{
		case 'h': case 'H': case '?': case '!':
		case KEY_F(1): case KEY_ESC: case KEY_EXIT:
			return 1;
		default:
			return 0;
	}
}

void fsHelp_stepper_init(struct fsHelp_stepper_engine *engine)
{
	helppage *cont;
	char contents_name[] = "Contents";

	if (!engine)
		return;

	engine->is_active = 1;
	plSetTextMode(plScrType);

	cont = brDecodeRef(contents_name);
	if (!cont)
		displaystr(1, 0, 0x04, "Help index missing", 18);

	brSetPage(cont);
	brSetWinStart(2);
	brSetWinHeight(plScrHeight - 2);
}

void fsHelp_stepper_finish(struct fsHelp_stepper_engine *engine)
{
	if (!engine)
		return;
	engine->is_active = 0;
}

int fsHelp_stepper_is_active(const struct fsHelp_stepper_engine *engine)
{
	return engine && engine->is_active;
}

struct fsHelp_stepper_engine *fsHelp_stepper_get_engine(void)
{
	return &g_fsHelp_engine;
}

unsigned char wasm_fsHelp2_arm(void)
{
	fsHelp_stepper_init(fsHelp_stepper_get_engine());
	return 1;
}

int fsHelp_stepper_run(struct fsHelp_stepper_engine *engine)
{
	if (!engine || !engine->is_active)
		return 1;

	make_title("opencp help", 0);
	brSetWinHeight(plScrHeight - 2);
	brDisplayHelp();
	framelock();

	while (Console.KeyboardHit())
	{
		uint16_t key = Console.KeyboardGetChar();

		if (key == VIRT_KEY_RESIZE)
			continue;
		if (fsHelp_stepper_leave(key))
		{
			fsHelp_stepper_finish(engine);
			return 1;
		}
		brHelpKey(key);
	}

	return 0;
}
