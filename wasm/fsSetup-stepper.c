/* OpenCP Module Player - fsSetup Stepper Implementation
 * Copyright (c) 2025-2026 Christophe Thibault - Non-blocking setup dialog for WASM
 *
 * This file provides a non-blocking version of fsSetup() that yields
 * to the browser after each frame, preventing the hang when ALT-C is pressed.
 *
 * Based on the modland stepper pattern from filesel/modland.com/wasm-stepper/
 */

#include "config.h"
#include "../types.h"
#include "../boot/console.h"
#include "../boot/psetting.h"
#include "../cpiface/cpiface.h"
#include "../stuff/poutput.h"
#include "../stuff/framelock.h"
#include "../filesel/pfilesel.h"
#include "fsSetup-stepper.h"

/* Global engine instance */
static struct fsSetup_stepper_engine g_fsSetup_engine = {0};

/* External variables from pfilesel.c. Screen mode is plScrType. */
extern int fsListScramble;
extern int fsListRemove;
extern int fsLoopMods;
extern int fsScanNames;
extern int fsScanArcs;
extern int fsScanInArc;
extern int fsWriteModInfo;
extern int fsEditWin;
extern int fsColorTypes;
extern int fsInfoMode;
extern int fsPutArcs;
extern int fsShowAllFiles;
extern int fsFPS;
extern int fsFPSCurrent;

void fsSetup_stepper_init(struct fsSetup_stepper_engine *engine)
{
	if (!engine) return;

	engine->is_active = 1;
	engine->first_run = 1;
	engine->stored = 0;
	engine->in_keyboard_help = 0;
	engine->last_fps_current = fsFPSCurrent;

	/* Set text mode like original fsSetup() */
	plSetTextMode(plScrType);
}

void fsSetup_stepper_finish(struct fsSetup_stepper_engine *engine)
{
	if (!engine) return;

	engine->is_active = 0;
	engine->first_run = 0;
	engine->stored = 0;
	engine->in_keyboard_help = 0;
}

int fsSetup_stepper_is_active(const struct fsSetup_stepper_engine *engine)
{
	return engine && engine->is_active;
}

struct fsSetup_stepper_engine *fsSetup_stepper_get_engine(void)
{
	return &g_fsSetup_engine;
}

/* Draw the setup dialog */
static void fsSetup_stepper_draw(struct fsSetup_stepper_engine *engine)
{
	const char *fsInfoModes[] = {
		"title, channels and size",
		"composer and date",
		"comment",
		"style and playtime",
		"long filenames"
	};
	const char *modename = plGetDisplayTextModeName();
	int i;

	make_title("file selector setup", 0);

	display_nprintf( 1, 0, 0x07, plScrWidth, "1:  screen mode: %.15o%s",                              modename);
	display_nprintf( 2, 0, 0x07, plScrWidth, "2:  scramble module list order: %.15o%s",               fsListScramble?"on":"off");
	display_nprintf( 3, 0, 0x07, plScrWidth, "3:  remove modules from playlist when played: %.15o%s", fsListRemove?"on":"off");
	display_nprintf( 4, 0, 0x07, plScrWidth, "4:  loop modules: %.15o%s",                             fsLoopMods?"on":"off");
	display_nprintf( 5, 0, 0x07, plScrWidth, "5:  scan module informatin: %.15o%s",                   fsScanNames?"on":"off");
	display_nprintf( 6, 0, 0x07, plScrWidth, "6:  scan archive contents: %.15o%s",                    fsScanArcs?"on":"off");
	display_nprintf( 7, 0, 0x07, plScrWidth, "7:  scan module information in archives: %.15o%s",      fsScanInArc?"on":"off");
	display_nprintf( 8, 0, 0x07, plScrWidth, "8:  save module information to disk: %.15o%s",          fsWriteModInfo?"on":"off");
	display_nprintf( 9, 0, 0x07, plScrWidth, "9:  edit window: %.15o%s",                              fsEditWin?"on":"off");
	display_nprintf(10, 0, 0x07, plScrWidth, "A:  module type colors: %.15o%s",                       fsColorTypes?"on":"off");
	display_nprintf(11, 0, 0x07, plScrWidth, "B:  module information display mode: %.15o%s",          fsInfoModes[fsInfoMode]);
	display_nprintf(12, 0, 0x07, plScrWidth, "C:  put archives: %.15o%s",                             fsPutArcs?"on":"off");
	display_nprintf(13, 0, 0x07, plScrWidth, "D:  show all files: %.15o%s",                           fsShowAllFiles?"on":"off");
	display_nprintf(14, 0, 0x07, plScrWidth, "+-: target framerate:%.15o%-4d%.7o, actual framerate: %.15o%d", fsFPS, engine->last_fps_current);

	displayvoid(15, 0, plScrWidth);

	displaystr(16, 0, 0x07, "ALT-S (or CTRL-S if in X) to save current setup to ocp.ini", plScrWidth);
	displaystr(plScrHeight-1, 0, 0x17, "  press the number of the item you wish to change and ESC when done", plScrWidth);

	displaystr(17, 0, 0x03, (engine->stored?"ocp.ini saved":""), plScrWidth);

	for (i=18; i < plScrHeight; i++)
	{
		displayvoid(i, 0, plScrWidth);
	}
}

int fsSetup_stepper_run(struct fsSetup_stepper_engine *engine)
{
	uint16_t c;

	if (!engine || !engine->is_active)
	{
		return 1;  /* Done */
	}

	/* Handle keyboard help display */
	if (engine->in_keyboard_help)
	{
		engine->in_keyboard_help = cpiKeyHelpDisplay();
		return 0;  /* Continue next frame */
	}

	/* Update FPS counter if it changed */
	if (fsFPSCurrent != engine->last_fps_current)
	{
		engine->last_fps_current = fsFPSCurrent;
	}

	/* Draw the dialog */
	fsSetup_stepper_draw(engine);

	/* Call framelock to ensure proper timing */
	framelock();

	/* Process ALL pending keyboard events (don't wait for input!) */
	while (Console.KeyboardHit())
	{
		c = Console.KeyboardGetChar();

		switch (c)
		{
			case '1':
				engine->stored = 0;
				plDisplaySetupTextMode();
				plScrType = Console.CurrentMode;
				break;
			case '2': engine->stored = 0; fsListScramble=!fsListScramble; break;
			case '3': engine->stored = 0; fsListRemove=!fsListRemove; break;
			case '4': engine->stored = 0; fsLoopMods=!fsLoopMods; break;
			case '5': engine->stored = 0; fsScanNames=!fsScanNames; break;
			case '6': engine->stored = 0; fsScanArcs=!fsScanArcs; break;
			case '7': engine->stored = 0; fsScanInArc=!fsScanInArc; break;
			case '8': engine->stored = 0; fsWriteModInfo=!fsWriteModInfo; break;
			case '9': engine->stored = 0; fsEditWin=!fsEditWin; break;
			case 'a': case 'A': engine->stored = 0; fsColorTypes=!fsColorTypes; break;
			case 'b': case 'B': engine->stored = 0; fsInfoMode=(fsInfoMode+1)%5; break;
			case 'c': case 'C': engine->stored = 0; fsPutArcs=!fsPutArcs; break;
			case 'd': case 'D': engine->stored = 0; fsShowAllFiles=!fsShowAllFiles; break;
			case '+': if (fsFPS<1000) fsFPS++; break;
			case '-': if (fsFPS>1) fsFPS--; break;
			case KEY_CTRL_S:
			case KEY_ALT_S:
			{
				const char *sec=cfGetProfileString(cfConfigSec, "fileselsec", "fileselector");

				cfSetProfileInt(cfScreenSec, "screentype", plScrType, 10);
				cfSetProfileBool(sec, "randomplay", fsListScramble);
				cfSetProfileBool(sec, "playonce", fsListRemove);
				cfSetProfileBool(sec, "loop", fsLoopMods);
				cfSetProfileBool(sec, "scanmodinfo", fsScanNames);
				cfSetProfileBool(sec, "scanarchives", fsScanArcs);
				cfSetProfileBool(sec, "scaninarcs", fsScanInArc);
				cfSetProfileBool(sec, "writeinfo", fsWriteModInfo);
				cfSetProfileBool(sec, "editwin", fsEditWin);
				cfSetProfileBool(sec, "typecolors", fsColorTypes);
				cfSetProfileBool(sec, "putarchives", fsPutArcs);
				cfSetProfileBool(sec, "showallfiles", fsShowAllFiles);
				cfSetProfileInt("screen", "fps", fsFPS, 10);
				cfStoreConfig();
				engine->stored = 1;
				break;
			}
			case VIRT_KEY_RESIZE:
				break;
			case KEY_EXIT:
			case KEY_ESC:
				/* Dialog is done */
				fsSetup_stepper_finish(engine);
				return 1;  /* Done */
			case KEY_ALT_K:
				cpiKeyHelpClear();
				cpiKeyHelp('1', "Toggle option 1");
				cpiKeyHelp('2', "Toggle option 2");
				cpiKeyHelp('3', "Toggle option 3");
				cpiKeyHelp('4', "Toggle option 4");
				cpiKeyHelp('5', "Toggle option 5");
				cpiKeyHelp('6', "Toggle option 6");
				cpiKeyHelp('7', "Toggle option 7");
				cpiKeyHelp('8', "Toggle option 8");
				cpiKeyHelp('9', "Toggle option 9");
				cpiKeyHelp('a', "Toggle option A");
				cpiKeyHelp('b', "Toggle option B");
				cpiKeyHelp('c', "Toggle option C");
				cpiKeyHelp('d', "Toggle option D");
				cpiKeyHelp('A', "Toggle option A");
				cpiKeyHelp('B', "Toggle option B");
				cpiKeyHelp('C', "Toggle option C");
				cpiKeyHelp('D', "Toggle option D");
				cpiKeyHelp('+', "Increase FPS");
				cpiKeyHelp('-', "Decrease FPS");
				cpiKeyHelp(KEY_ALT_S, "Store settings to ocp.ini");
				cpiKeyHelp(KEY_CTRL_S, "Store settings to ocp.ini (avoid this key if in curses)");
				engine->in_keyboard_help = 1;
				break;
		}
	}

	/* Continue on next frame */
	return 0;
}
