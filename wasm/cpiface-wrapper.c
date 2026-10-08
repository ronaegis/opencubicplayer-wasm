/* cpiface-wrapper.c - WASM-specific cpiface behavior wrapper
 *
 * PURPOSE:
 *   Wraps cpiface.c static functions to add WASM-specific behavior:
 *   - Stepper-based rendering (non-blocking event loop)
 *   - Audio latency compensation for pattern view
 *   - State management for file close/reset
 *
 * WHY MACRO-INCLUDE APPROACH:
 *   - cpiface.c functions are 'static' (internal linkage)
 *   - Cannot use weak symbols (Emscripten bugs in MAIN_MODULE mode)
 *   - Cannot use --wrap (doesn't work within same compilation unit)
 *   - Cannot modify original cpiface.c (project policy)
 *   - Preprocessor rename is only approach that works
 *
 * MAINTENANCE:
 *   All validation runs automatically with: cd wasm/tests && npm test
 *   See wasm/CPIFACE-WRAPPER.md for details on adding new wrapped functions.
 *
 * VALIDATION LAYERS:
 *   1. Compile-time: Static assertions, symbol existence checks
 *   2. Build-time: Symbol export verification (npm test)
 *   3. Runtime: Circular reference checks
 *
 * Last updated: 2025-10-15
 */

#include "cpiface_stepper.h"
#include "dev/mcp.h"
#include <stdio.h>

/* Rename internal symbols from cpiface.c so we can wrap behaviour without
 * modifying the original source. */
#define plmpCallBack            cpiface_original_plmpCallBack
#define plmpOpenScreen          cpiface_original_plmpOpenScreen
#define plmpDrawScreen          cpiface_original_plmpDrawScreen
#define plmpCloseScreen         cpiface_original_plmpCloseScreen
#define plmpOpenFile            cpiface_original_plmpOpenFile
#define plmpCloseFile           cpiface_original_plmpCloseFile
#define plmpLateInit            cpiface_original_plmpLateInit
#define plmpInit                cpiface_original_plmpInit
#define plmpPreClose            cpiface_original_plmpPreClose
#define plmpClose               cpiface_original_plmpClose
#define plOpenCP                cpiface_original_plOpenCP
#define dllextinfo              cpiface_original_dllextinfo
#define cpiDebugRun             cpiface_original_cpiDebugRun
#define curplayer               cpiface_original_curplayer
#define cpifaceSessionAPI       cpiface_original_cpifaceSessionAPI

#include "../cpiface/cpiface.c"

#undef plmpCallBack
#undef plmpOpenScreen
#undef plmpDrawScreen
#undef plmpCloseScreen
#undef plmpOpenFile
#undef plmpCloseFile
#undef plmpLateInit
#undef plmpInit
#undef plmpPreClose
#undef plmpClose
#undef plOpenCP
#undef dllextinfo
#undef cpiDebugRun
#undef curplayer
#undef cpifaceSessionAPI

// ============================================================================
// COMPILE-TIME VALIDATION
// This ensures the macro-include trick is working correctly and catches
// issues if cpiface.c changes in incompatible ways.
// ============================================================================

/* Verify cpiface.c was actually included */
#ifndef __CPIFACE_H
#error "cpiface.h not included - check include path"
#endif

/* Verify renamed symbols exist by taking their address.
 * This function is never called, but forces compile-time validation.
 * If cpiface.c removes a function, this will fail to compile. */
static void __attribute__((unused)) __validate_renamed_symbols(void) {
	volatile void *check[] = {
		(void*)&cpiface_original_plmpCallBack,
		(void*)&cpiface_original_plmpOpenScreen,
		(void*)&cpiface_original_plmpDrawScreen,
		(void*)&cpiface_original_plmpCloseScreen,
		(void*)&cpiface_original_plmpOpenFile,
		(void*)&cpiface_original_plmpCloseFile,
		(void*)&cpiface_original_plmpLateInit,
		(void*)&cpiface_original_plmpInit,
		(void*)&cpiface_original_plmpPreClose,
		(void*)&cpiface_original_plmpClose,
		(void*)&cpiface_original_plOpenCP,
		(void*)&cpiface_original_cpiDebugRun,
		(void*)&cpiface_original_curplayer,
		(void*)&cpiface_original_cpifaceSessionAPI,
		(void*)&cpiface_original_dllextinfo,
	};
	(void)check; // Suppress unused warning
}

/* Verify critical structures have expected layout */
_Static_assert(sizeof(struct cpifaceSessionPrivate_t) > 0,
               "cpifaceSessionPrivate_t not defined - cpiface.c include failed");

_Static_assert(sizeof(struct linkinfostruct) > 0,
               "linkinfostruct not defined - missing includes");

/* Verify we're wrapping the right version */
#ifndef DLLVERSION
#error "DLLVERSION not defined - cpiface structures may be incompatible"
#endif

// ============================================================================
// WASM ERROR DIALOG STEPPER
// Non-blocking version of cpiDebugRun() compatible with emscripten main loop
// ============================================================================

static struct {
    int active;       // Is the error dialog currently displayed?
    int mlScroll;     // Scroll position in the error message
} wasm_debug_stepper = {0, 0};

// Non-blocking version of cpiDebugRun() - call this every frame
// Returns: 1 if dialog is still showing, 0 if user dismissed it
static int wasm_cpiDebugRun_tick(void)
{
    int i;
    int mlWidth, mlHeight, mlLeft, mlTop;
    // Safety check: make sure console is valid before drawing
    if (!cpiface_original_cpifaceSessionAPI.Public.console) {
        fprintf(stderr, "WASM: Error dialog cannot be displayed - console not initialized\n");
        return 0; // Dismiss immediately to avoid crash
    }

    if (!wasm_debug_stepper.active) {
        // First frame - initialize
        wasm_debug_stepper.active = 1;
        wasm_debug_stepper.mlScroll = 0;
        plSetTextMode(plScrType);
    }

    // Calculate dialog dimensions (same as cpiDebugRun)
    mlWidth = MIN (cpiface_original_cpifaceSessionAPI.Public.console->TextWidth - 2, 122);

    // Recalculate lines if width changed
    if (cpiface_original_cpifaceSessionAPI.cpiDebugLastWidth != (mlWidth - 2))
    {
        cpiDebugRecalcLines(mlWidth - 2);
    }

    mlHeight = MAX ( MIN ( cpiface_original_cpifaceSessionAPI.Public.console->TextHeight - 2,
                           cpiface_original_cpifaceSessionAPI.cpiDebug_lines + 2 ), 10);

    while (((wasm_debug_stepper.mlScroll + mlHeight - 2) > cpiface_original_cpifaceSessionAPI.cpiDebug_lines) && wasm_debug_stepper.mlScroll)
    {
        wasm_debug_stepper.mlScroll--;
    }

    mlLeft = (cpiface_original_cpifaceSessionAPI.Public.console->TextWidth - mlWidth) / 2;
    mlTop = (cpiface_original_cpifaceSessionAPI.Public.console->TextHeight - mlHeight) / 2;

    // Draw the error dialog (same as cpiDebugRun)
    for (i=0; i < cpiface_original_cpifaceSessionAPI.Public.console->TextHeight; i++)
    {
        if ((i < mlTop) || (i >= mlTop + mlHeight))
        {
            cpiface_original_cpifaceSessionAPI.Public.console->Driver->DisplayVoid (i, 0, cpiface_original_cpifaceSessionAPI.Public.console->TextWidth);
        } else {
            if (i == mlTop)
            {
                cpiface_original_cpifaceSessionAPI.Public.console->DisplayPrintf (i, 0, 0x01, cpiface_original_cpifaceSessionAPI.Public.console->TextWidth, "%*C \xc9%*C\xcd\xbb", mlLeft, mlWidth - 2);
            } else if (i == (mlTop + mlHeight - 1))
            {
                cpiface_original_cpifaceSessionAPI.Public.console->DisplayPrintf (i, 0, 0x01, cpiface_original_cpifaceSessionAPI.Public.console->TextWidth, "%*C \xc8%*C\xcd\xbc", mlLeft, mlWidth - 2);
            } else if ((i - mlTop + wasm_debug_stepper.mlScroll - 1) >= cpiface_original_cpifaceSessionAPI.cpiDebug_lines)
            {
                cpiface_original_cpifaceSessionAPI.Public.console->DisplayPrintf (i, 0, 0x01, cpiface_original_cpifaceSessionAPI.Public.console->TextWidth, "%*C \xba%*C \xba", mlLeft, mlWidth - 2);
            } else {
                cpiface_original_cpifaceSessionAPI.Public.console->DisplayPrintf (i, 0, 0x01, cpiface_original_cpifaceSessionAPI.Public.console->TextWidth, "%*C \xba%0.*o%*.*s%0.1o\xba",
                        /* how many spaces to insert */
                        mlLeft,
                        /* color to use on the text */
                        (((cpiface_original_cpifaceSessionAPI.cpiDebug_bufbase + cpiface_original_cpifaceSessionAPI.cpiDebug_line[i - mlTop + wasm_debug_stepper.mlScroll - 1].offset)[0] == '[') ||
                         cpiface_original_cpifaceSessionAPI.cpiDebug_line[i - mlTop + wasm_debug_stepper.mlScroll - 1].linebreak ) ? 3 : 12,
                        /* target text width */
                        mlWidth - 2,
                        /* source text width */
                        cpiface_original_cpifaceSessionAPI.cpiDebug_line[i - mlTop + wasm_debug_stepper.mlScroll - 1].length,
                        /* source text data */
                        cpiface_original_cpifaceSessionAPI.cpiDebug_bufbase + cpiface_original_cpifaceSessionAPI.cpiDebug_line[i - mlTop + wasm_debug_stepper.mlScroll - 1].offset
                );
            }
        }
    }

    framelock();

    // Check for keyboard input
    while (Console.KeyboardHit())
    {
        uint16_t key = Console.KeyboardGetChar();
        if (key == KEY_UP)
        {
            if (wasm_debug_stepper.mlScroll)
            {
                wasm_debug_stepper.mlScroll--;
            }
        } else if (key == KEY_DOWN)
        {
            if (((wasm_debug_stepper.mlScroll + 1 + mlHeight - 2) <= cpiface_original_cpifaceSessionAPI.cpiDebug_lines))
            {
                wasm_debug_stepper.mlScroll++;
            }
        } else if (
            ((key >= 'A') && (key <= 'Z')) ||
            ((key >= 'a') && (key <= 'z')) ||
            ((key >= '0') && (key <= '9')) ||
            (key == _KEY_ENTER)            ||
            (key == ' ')                   ||
            (key == KEY_ESC) )
        {
            // User pressed a key to dismiss - reset and return
            wasm_debug_stepper.active = 0;
            wasm_debug_stepper.mlScroll = 0;
            return 0; // Dialog dismissed
        } else if (key == KEY_EXIT)
        {
            wasm_debug_stepper.active = 0;
            wasm_debug_stepper.mlScroll = 0;
            return 0; // Dialog dismissed
        }
    }

    return 1; // Dialog still showing
}

static void wasm_cpiDebugRun_reset(void)
{
    wasm_debug_stepper.active = 0;
    wasm_debug_stepper.mlScroll = 0;
}

// ============================================================================

static struct cpiface_stepper_state cpiface_internal_stepper_state;
static int wasm_error_dialog_active = 0;

extern void wasm_install_scope_latency_wrappers(struct cpifaceSessionAPI_t *session);
extern void wasm_scope_latency_reset(void);

struct cpifaceSessionPrivate_t;
extern struct cpifaceSessionPrivate_t cpiface_original_cpifaceSessionAPI;
extern struct cpifaceSessionPrivate_t cpifaceSessionAPI __attribute__((alias("cpiface_original_cpifaceSessionAPI")));

// Store original mixer's mcpGet for wrapping
static int (*original_mixer_mcpGet)(struct cpifaceSessionAPI_t *, int, int) = NULL;

// WASM mcpGet wrapper that applies pattern view latency
static int wasm_mcpGet_latency_wrapper(struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt) {
    int real_value;

    if (!original_mixer_mcpGet) {
        return 0;
    }

    real_value = original_mixer_mcpGet(cpifaceSession, ch, opt);

    // Apply pattern view latency offset to read timer only
    // Timer is in units of 1/65536 seconds, so convert ms to these units
    // Subtracting from the read timer makes pattern lag behind audio
    if (ch == -1 && opt == mcpGTimer) {
        int latency_ms = wasm_get_current_audio_latency_ms();
        int offset_ticks = (latency_ms * 65536) / 1000;
        real_value -= offset_ticks;
    }

    return real_value;
}

void cpiface_stepper_reset(struct cpiface_stepper_state *state)
{
    if (!state) {
        return;
    }

    if (state->screen_open) {
        cpiface_original_plmpCloseScreen();
    }

    state->screen_open = 0;
    state->stop = interfaceReturnContinue;
    state->initialized = 1;
}

void cpiface_stepper_init(struct cpiface_stepper_state *state)
{
    cpiface_stepper_reset(state);
}

// Track error dialog state for non-blocking display
static int cpiface_error_dialog_showing = 0;

static interfaceReturnEnum cpiface_stepper_handle_no_player(struct cpiface_stepper_state *state)
{
    // First call - set up the error dialog state
    if (!cpiface_error_dialog_showing) {
        cpiface_error_dialog_showing = 1;
        wasm_debug_stepper.active = 0; // Reset to ensure clean start
    }

    // Run the non-blocking error dialog tick
    int still_showing = wasm_cpiDebugRun_tick();

    if (!still_showing) {
        // User dismissed the dialog
        cpiface_error_dialog_showing = 0;
        wasm_cpiDebugRun_reset();
        cpiface_stepper_reset(state);
        return interfaceReturnNextAuto;
    }

    // Dialog still showing - continue displaying
    return interfaceReturnContinue;
}

interfaceReturnEnum cpiface_stepper_tick(struct cpiface_stepper_state *state)
{
    extern struct cpifaceSessionPrivate_t cpifaceSessionAPI;

    if (!state || !state->initialized) {
        cpiface_stepper_init(state);
    }

    if (!cpiface_original_curplayer) {
        return cpiface_stepper_handle_no_player(state);
    }

    if (!state->screen_open) {
        cpiface_original_plmpOpenScreen();

        // After opening screen, intercept the mixer's mcpGet with our latency wrapper
        if (cpifaceSessionAPI.Public.mcpGet && cpifaceSessionAPI.Public.mcpGet != wasm_mcpGet_latency_wrapper) {
            original_mixer_mcpGet = cpifaceSessionAPI.Public.mcpGet;
            cpifaceSessionAPI.Public.mcpGet = wasm_mcpGet_latency_wrapper;
        }

        wasm_install_scope_latency_wrappers(&cpifaceSessionAPI.Public);

        state->stop = interfaceReturnContinue;
        state->screen_open = 1;
    }

    wasm_install_scope_latency_wrappers(&cpifaceSessionAPI.Public);

    if (state->stop == interfaceReturnContinue) {
        state->stop = cpiface_original_plmpDrawScreen();
        if (state->stop != interfaceReturnContinue) {
            cpiface_original_plmpCloseScreen();
            state->screen_open = 0;
        }
    }

    if ((state->stop != interfaceReturnContinue) && (state->screen_open == 0)) {
        interfaceReturnEnum result = state->stop;
        state->stop = interfaceReturnContinue;
        return result;
    }

    return state->stop;
}

interfaceReturnEnum cpiface_stepper_run_blocking(struct cpiface_stepper_state *state)
{
    interfaceReturnEnum result;

    do {
        result = cpiface_stepper_tick(state);
    } while (result == interfaceReturnContinue);

    return result;
}

static interfaceReturnEnum wasm_plmpCallBack(void)
{
    // If error dialog is active, run the non-blocking error display
    if (wasm_error_dialog_active) {
        int still_showing = wasm_cpiDebugRun_tick();
        if (!still_showing) {
            // User dismissed the dialog - signal quit to close interface
            wasm_error_dialog_active = 0;
            wasm_cpiDebugRun_reset();
            return interfaceReturnQuit;
        }
        return interfaceReturnContinue; // Keep showing dialog
    }

    // Normal playback path
    if (!cpiface_internal_stepper_state.initialized) {
        cpiface_stepper_init(&cpiface_internal_stepper_state);
    }

    return cpiface_stepper_run_blocking(&cpiface_internal_stepper_state);
}

static void wasm_plmpCloseFile(void)
{
    cpiface_stepper_reset(&cpiface_internal_stepper_state);
    wasm_scope_latency_reset();
    wasm_error_dialog_active = 0;
    cpiface_error_dialog_showing = 0;
    wasm_cpiDebugRun_reset();
    cpiface_original_plmpCloseFile();
}

static int wasm_plmpOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *fi, const struct cpifaceplayerstruct *cp)
{
    int result = cpiface_original_plmpOpenFile(info, fi, cp);

    // When file fails to load, curplayer will be 0 but result is still 1.
    // The error dialog will be shown by cpiface_stepper_tick via
    // cpiface_stepper_handle_no_player, which uses the non-blocking
    // wasm_cpiDebugRun_tick instead of the blocking cpiDebugRun.
    //
    // Also print to stderr for console visibility
    if (result && !cpiface_original_curplayer) {
        if (cpiface_original_cpifaceSessionAPI.cpiDebug_buffill > 0) {
            fprintf(stderr, "WASM Error loading file:\n%s\n", cpiface_original_cpifaceSessionAPI.cpiDebug_bufbase);
        }
    }

    return result;
}

static int wasm_plmpLateInit(struct PluginInitAPI_t *API)
{
    // Runtime validation: verify our wrappers are properly installed
    if (cpiface_original_plOpenCP.Run == wasm_plmpCallBack) {
        fprintf(stderr, "ERROR: cpiface-wrapper.c: Circular reference detected in Run callback\n");
        return -1;
    }

    if (cpiface_original_plOpenCP.Close == wasm_plmpCloseFile) {
        fprintf(stderr, "ERROR: cpiface-wrapper.c: Circular reference detected in Close callback\n");
        return -1;
    }

    if (cpiface_original_plOpenCP.Init == wasm_plmpOpenFile) {
        fprintf(stderr, "ERROR: cpiface-wrapper.c: Circular reference detected in Init callback\n");
        return -1;
    }

    cpiface_stepper_reset(&cpiface_internal_stepper_state);
    cpiface_original_plOpenCP.Init = wasm_plmpOpenFile;
    cpiface_original_plOpenCP.Run = wasm_plmpCallBack;
    cpiface_original_plOpenCP.Close = wasm_plmpCloseFile;
    return cpiface_original_plmpLateInit(API);
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {
    .name = "cpiface",
    .desc = "OpenCP Interface (c) 1994-'25 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad",
    .ver = DLLVERSION,
    .sortindex = 35,
    .Init = cpiface_original_plmpInit,
    .LateInit = wasm_plmpLateInit,
    .PreClose = cpiface_original_plmpPreClose,
    .Close = cpiface_original_plmpClose
};

extern struct linkinfostruct cpiface_dllextinfo __attribute__((alias("dllextinfo")));

// Helper function to check if a player was successfully loaded
// This is needed because plmpOpenFile returns 1 even when the player fails,
// but sets curplayer to NULL in that case.
int wasm_cpiface_has_valid_player(void)
{
    return cpiface_original_curplayer != NULL;
}
