/* OpenCP Module Player - WASM Interface Main
 * Copyright (C) 2025 Christophe Thibault
 *
 * This file provides the WASM wrapper around OCP's original interface system
 * to display the full original player interface when a MOD is loaded.
 *
 * The strategy is to use the existing OCP interface system with minimal changes:
 * - Use the same pfsmain.c logic but adapted for WASM
 * - Load files from WASM memory instead of filesystem
 * - Use the original plOpenCP interface for full feature set
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include "types.h"

// OCP headers
#include "boot/pmain.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "dev/deviplay.h"
#include "dev/deviwave.h"
#include "dev/postproc.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "playxm/xmtype.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "stuff/poll.h"

// WASM-specific headers
#include "wasm-fileio.h"
#include "cpiface_stepper.h"

void wasm_enable_file_selector(void);
void wasm_enable_file_selector_overlay(void);

// Provide file selector helpers from the WASM stubs layer
extern void wasm_set_single_file(struct ocpfilehandle_t *file);
extern struct ocpfilehandle_t *wasm_file_open_readfile(const char *path);

// Global state for the interface
static struct moduleinfostruct current_module_info;
static struct ocpfilehandle_t *current_file = NULL;
static const struct interfacestruct *current_interface = NULL;
static const struct cpifaceplayerstruct *current_player = NULL;
static interfaceReturnEnum interface_state = interfaceReturnContinue;
static int interface_initialized = 0;
static struct cpiface_stepper_state wasm_cpiface_stepper;
static int pattern_view_activation_frames = 0;
static int interface_uses_cpiface = 0;

// Flag to control main loop
static int should_stop = 0;

// Forward declarations
static int wasm_load_and_init_interface(const char *virtual_path);
static void wasm_close_interface(void);

// WASM exported functions
EMSCRIPTEN_KEEPALIVE
int load_module_file_interface(const char *virtual_path) {
    // Close any existing interface
    if (interface_initialized) {
        wasm_close_interface();
    }

    // Load and initialize the interface
	if (wasm_load_and_init_interface(virtual_path) != 0) {
		fprintf(stderr, "WASM: Failed to load module via interface\n");
		return -1;
	}

    return 0;
}

static int wasm_interface_finalize_init(void) {
	// CRITICAL: Initialize text mode system and activate pattern view
	// First ensure text mode is properly initialized
	extern void cpiTextSetMode(struct cpifaceSessionAPI_t *cpifaceSession, const char *name);
	extern struct cpifaceSessionPrivate_t cpifaceSessionAPI;
	extern void cpiSetMode(const char *mode);

	// Initialize text mode system
	Console.Driver->SetTextMode(8); // 8 = text mode

	cpiSetMode("text");

	cpiTextSetMode(&cpifaceSessionAPI.Public, "trak");

	// Verify Console.VidMem is allocated for text rendering
	if (!Console.VidMem) {
		fprintf(stderr, "WASM: Console.VidMem is NULL; expected SDL2 to allocate it\n");
		return -1;
	}

    interface_initialized = 1;
    interface_state = interfaceReturnContinue;
    should_stop = 0;
    cpiface_stepper_init(&wasm_cpiface_stepper);
    pattern_view_activation_frames = 0;
    return 0;
}

int wasm_interface_load_from_handle(const struct moduleinfostruct *info,
                                    struct ocpfilehandle_t *filehandle,
                                    const struct interfacestruct *interface,
                                    const struct cpifaceplayerstruct *player)
{
	if (!filehandle || !interface) {
		fprintf(stderr, "WASM: Invalid arguments passed to wasm_interface_load_from_handle\n");
		return -1;
	}

    // Close any existing interface first
    if (interface_initialized) {
        wasm_close_interface();
    }

    memset(&current_module_info, 0, sizeof(current_module_info));
    if (info) {
        current_module_info = *info;
    }

    current_file = filehandle;
    if (current_file->ref) {
        current_file->ref(current_file);
    }

    // Reset the file position before handing it to the interface
    if (current_file->seek_set) {
        current_file->seek_set(current_file, 0);
    }

    wasm_set_single_file(current_file);

    current_interface = interface;
    current_player = player;
    interface_uses_cpiface = (player != NULL);

	if (!current_interface->Init(&current_module_info, current_file, current_player)) {
		fprintf(stderr, "WASM: Interface initialization failed\n");
        wasm_set_single_file(NULL);
        if (current_file && current_file->unref) {
            current_file->unref(current_file);
        }
        current_file = NULL;
        current_interface = NULL;
        current_player = NULL;
        return -1;
    }

    // Check if a player was actually loaded (plmpOpenFile returns 1 even on failure,
    // but sets curplayer to NULL). In that case, we need to let cpiface_stepper_tick
    // handle showing the error dialog instead of calling wasm_interface_finalize_init.
    extern int wasm_cpiface_has_valid_player(void);
    int has_valid_player = wasm_cpiface_has_valid_player();

	if (interface_uses_cpiface && has_valid_player) {
        if (wasm_interface_finalize_init() < 0) {
            if (current_interface && current_interface->Close) {
                current_interface->Close();
            }
            wasm_set_single_file(NULL);
            if (current_file && current_file->unref) {
                current_file->unref(current_file);
            }
            current_file = NULL;
            current_interface = NULL;
            current_player = NULL;
            interface_uses_cpiface = 0;
            return -1;
        }
    } else {
        interface_initialized = 1;
        interface_state = interfaceReturnContinue;
        should_stop = 0;
        cpiface_stepper_reset(&wasm_cpiface_stepper);
        pattern_view_activation_frames = 0;
    }

    return 0;
}

// Load module and initialize the full OCP interface
static int wasm_load_and_init_interface(const char *virtual_path) {
    struct moduletype modtype;
    const char *filename = virtual_path;
    struct ocpfilehandle_t *filehandle = NULL;
    struct moduleinfostruct module_info;
    const struct interfacestruct *interface = NULL;
    const struct cpifaceplayerstruct *player = NULL;

	if (!virtual_path) {
		fprintf(stderr, "WASM: Invalid virtual path passed to wasm interface\n");
		return -1;
	}

    const char *basename = strrchr(virtual_path, '/');
    if (basename && basename[1]) {
        filename = basename + 1;
    }

    // Create file handle from the virtual filesystem
	filehandle = wasm_file_open_readfile(virtual_path);
	if (!filehandle) {
		fprintf(stderr, "WASM: Failed to open virtual file '%s'\n", virtual_path);
		return -1;
	}

    // Fill in initial module info
    memset(&module_info, 0, sizeof(module_info));
    strncpy(module_info.title, filename, sizeof(module_info.title) - 1);
    module_info.modtype.integer.i = mtUnRead;
    module_info.flags = 0;

    // Use proper OCP file type detection
    extern int mdbReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f);
	if (mdbReadInfo(&module_info, filehandle)) {
		fprintf(stderr, "WASM: File type detection failed or returned unknown type\n");
	}
    if (filehandle->seek_set) {
        filehandle->seek_set(filehandle, 0); // Reset file position after scanning
    }

    modtype = module_info.modtype;
	// Find the appropriate interface for detected module type
    plFindInterface(modtype, &interface, &player);

	if (!interface) {
		fprintf(stderr, "WASM: No interface found for module type 0x%08x\n", modtype.integer.i);
		if (filehandle->unref) {
			filehandle->unref(filehandle);
		}
		return -1;
	}

	if (!player) {
		fprintf(stderr, "WASM: No player registered for module type 0x%08x; continuing with interface only\n",
		        modtype.integer.i);
	}

    if (wasm_interface_load_from_handle(&module_info, filehandle, interface, player) < 0) {
        if (filehandle->unref) {
            filehandle->unref(filehandle);
        }
        return -1;
    }

    // wasm_interface_load_from_handle() takes its own reference, drop ours
    if (filehandle->unref) {
        filehandle->unref(filehandle);
    }
    return 0;
}

// Close the current interface
static void wasm_close_interface(void) {
    if (interface_initialized && current_interface) {
        current_interface->Close();
        interface_initialized = 0;
    }

    if (current_file) {
        wasm_set_single_file(NULL);
        current_file->unref(current_file);
        current_file = NULL;
    }

    current_interface = NULL;
    current_player = NULL;
    interface_uses_cpiface = 0;
    interface_state = interfaceReturnContinue;
    cpiface_stepper_reset(&wasm_cpiface_stepper);
    pattern_view_activation_frames = 0;
}

// Return to file selector - called when user presses 'F' key during playback
static void wasm_return_to_file_selector(void) {
    /* Do NOT close the interface - playback continues in background
     * The console state switching is handled by the file selector itself
     * Audio continues via the mixer which runs independently */

    /* Switch to overlay mode - this allows both file selector and playback to run */
    wasm_enable_file_selector_overlay();

    /* Do NOT set should_stop - we want to keep the interface running
     * The main loop in wasm-original-main.c will handle both modes concurrently */
}

// Main loop - this is called by emscripten at 60fps
void wasm_interface_main_loop(void) {
	// Exit if stop requested
	if (should_stop) {
		if (interface_initialized) {
			wasm_close_interface();
		}
		return;
	}

	// If no interface is active, just return
	if (!interface_initialized || !current_interface) {
		return;
	}

	if (interface_uses_cpiface) {
		interface_state = cpiface_stepper_tick(&wasm_cpiface_stepper);

		// Ensure pattern view stays active after interface runs
		// Only do this if we have a valid player - otherwise modes aren't registered
		extern int wasm_cpiface_has_valid_player(void);
		if (wasm_cpiface_has_valid_player() && pattern_view_activation_frames < 5) { // Try for first 5 frames
			extern void cpiTextSetMode(struct cpifaceSessionAPI_t *cpifaceSession, const char *name);
			extern struct cpifaceSessionPrivate_t cpifaceSessionAPI;
			cpiTextSetMode(&cpifaceSessionAPI.Public, "trak");
			pattern_view_activation_frames++;
		}
	} else {
		interface_state = current_interface->Run();
	}

	/* Present the frame through the video poll. RefreshScreenText is static
	 * inside the SDL2 backend and runs from sdl2VideoTimer. */
	tmTimerHandler(pollTypeVideo);

	switch (interface_state) {
		case interfaceReturnContinue:
			// Continue running
			break;

		case interfaceReturnQuit:
			should_stop = 1;
			break;

		case interfaceReturnNextAuto:
		case interfaceReturnNextManuel:
		case interfaceReturnPrevManuel:
			if (!interface_uses_cpiface) {
				wasm_close_interface();
				wasm_enable_file_selector();
				return;
			}
			// Check if we have a valid player - if not, this is the error dialog dismissing
			extern int wasm_cpiface_has_valid_player(void);
			if (!wasm_cpiface_has_valid_player()) {
				wasm_close_interface();
				wasm_enable_file_selector();
				return;
			}
			interface_state = interfaceReturnContinue;
			break;

		case interfaceReturnCallFs:
			wasm_return_to_file_selector();
			// After file selector returns, interface_state will be reset
			break;

		case interfaceReturnDosShell:
			interface_state = interfaceReturnContinue;
			break;
	}
}

// Initialize the WASM interface system
int wasm_interface_init(void) {

    // Initialize OCP subsystems needed for the interface
    // This is similar to what pfsmain.c does, but simplified

    // CRITICAL: Initialize plpalette for text rendering
    // SDL2 and WASM don't call vgaMakePal() from console.c, so we need to do it here
    extern unsigned char plpalette[256];
    int bg, fg;
	for (bg = 0; bg < 16; bg++) {
		for (fg = 0; fg < 16; fg++) {
			plpalette[16 * bg + fg] = 16 * bg + fg; // Identity mapping: 0x07 -> 0x07
		}
	}

    // File system initialization is now handled in wasm-original-main.c
    // (fsPreInit, fsInit, fsLateInit are called there with proper configAPI)

    // SDL owns canvas resizing, framebuffer allocation and VIRT_KEY_RESIZE.
    // Do not change Console dimensions from browser event callbacks: the
    // renderer may still be using the framebuffer allocated for the old size.

    return 0;
}

// Start the main loop
void wasm_interface_start(void) {
}
