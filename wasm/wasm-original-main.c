/* OpenCP Module Player - WASM Main Entry Point
 * Copyright (c) 2025-2026 Christophe Thibault - WASM port using original DOS interface
 *
 * This file creates a WASM main entry point that uses the original
 * OpenCubicPlayer DOS interface files with minimal modifications
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include <SDL.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#include "../types.h"
#include "idbfs-persistence.h"
#include "loading-progress.h"

// Include original OCP headers
#include "../boot/pmain.h"
#include "../boot/psetting.h"
#include "../boot/console.h"
#include "../boot/plinkman.h"
#include "../cpiface/cpiface.h"
#include "../cpiface/cpiface-private.h"
#include "../stuff/poutput.h"
#include "../stuff/poutput-sdl2.h"
#include "../stuff/framelock.h"
#include "../playxm/xmplay.h"
#include "../dev/player.h"
#include "../dev/mcp.h"
#include "../dev/deviplay.h"
#include "../filesel/filesystem-drive.h"
#include "../filesel/dirdb.h"
#include "../filesel/pfilesel.h"
#include "../filesel/filesystem.h"
#include "../filesel/filesystem-setup.h"
#include "../filesel/filesystem-file-dev.h"
#include <string.h>

#ifdef OCP_WASM_FILESEL_STEPPER
int fsFileSelectIsActive(void);
#else
static inline int fsFileSelectIsActive(void) { return 0; }
#endif

int wasm_interface_load_from_handle(const struct moduleinfostruct *info,
                                    struct ocpfilehandle_t *filehandle,
                                    const struct interfacestruct *interface,
                                    const struct cpifaceplayerstruct *player);

// Forward declarations for statically linked modules
extern struct linkinfostruct cpiface_dllextinfo;
extern struct linkinfostruct cphelper_dllextinfo;
extern struct linkinfostruct cphlpif_dllextinfo;
extern struct linkinfostruct deviplay_dllextinfo;
extern struct linkinfostruct deviwave_dllextinfo;
extern void wasm_audio_tick(void);

// Global plugin lifecycle state
struct PluginInitAPI_t wasm_plugin_api = {0};
static struct PluginCloseAPI_t wasm_plugin_close_api = {0};
static int wasm_plugins_initialized;
static int wasm_runtime_started;
static int wasm_ocp_main_called;
int stored_argc;  /* Non-static for platform API access */
char **stored_argv;  /* Non-static for platform API access */
static int wasm_startup_completed;

/* Runtime mode state machine for WASM
 * Defines the current operating mode and manages transitions between
 * file selector and playback modes.
 */
enum wasm_runtime_mode {
	WASM_MODE_INIT,              /* Initial state, not yet ready */
	WASM_MODE_FILESEL,           /* File selector active */
	WASM_MODE_FILESEL_SELECTED,  /* File selected, ready to load */
	WASM_MODE_PLAYBACK,          /* Playing a file */
	WASM_MODE_PLAYBACK_WITH_FILESEL, /* File selector shown while playback continues */
	WASM_MODE_VIRTUAL_INTERFACE, /* Running virtual interface (no audio playback) */
	WASM_MODE_ERROR              /* Error state */
};

struct wasm_runtime_state {
	enum wasm_runtime_mode mode;
	struct moduleinfostruct pending_module_info;
	struct ocpfilehandle_t *pending_filehandle;
};

static struct wasm_runtime_state g_runtime = {
	.mode = WASM_MODE_INIT,
	.pending_filehandle = NULL
};



// WASM-side extension registry for JavaScript access
static char **wasm_registered_extensions = NULL;
static size_t wasm_registered_extensions_count = 0;
static size_t wasm_registered_extensions_capacity = 0;

void wasm_display_shutdown(void);
extern void wasm_interface_main_loop(void);

// WASM stub function declarations
int plDisplayInit(void);
int conInit(void);
int errInit(void);

// External API structures and functions
extern struct configAPI_t configAPI;
extern const struct dirdbAPI_t dirdbAPI;

// Registration function declarations
void fsTypeRegister(struct moduletype modtype, const char **description, const char *interfacename, const struct cpifaceplayerstruct *cp);
void fsRegisterExt(const char *ext);
void plrRegisterDriver(const struct plrDriver_t *driver);
void mcpRegisterDriver(const struct mcpDriver_t *driver);

/* Legacy path definitions - now managed by psetting-platform API */
#include "psetting-platform.h"

/* Helper macros for platform paths (use platform API at runtime) */
#define WASM_HOME_ROOT          "/home"
#define WASM_USER_ROOT          "/home/web_user"
#define WASM_CONFIG_DIR         "/home/web_user/.ocp"
#define WASM_DATAHOME_DIR       "/home/web_user/.ocp/data"

static int wasm_ensure_directory(const char *path)
{
    if (!path) {
        return -1;
    }

	if (mkdir(path, 0777) && errno != EEXIST) {
		fprintf(stderr, "WASM: mkdir(%s) failed: %d\n", path, errno);
		return -1;
	}

    return 0;
}

static int wasm_prepare_directories(void)
{
    /* Note: WASM_CONFIG_DIR and WASM_DATAHOME_DIR are NOT created here
     * because they are subdirectories of the IDBFS mount point.
     * IDBFS will handle their creation when it mounts at /home/web_user/.ocp
     */
    const char *paths[] = {
        WASM_HOME_ROOT,        /* /home */
        WASM_USER_ROOT,        /* /home/web_user */
        /* WASM_CONFIG_DIR is NOT listed - IDBFS creates it */
        /* WASM_DATAHOME_DIR is NOT listed - IDBFS creates it */
        "/program",
        "/program/autoload",
        "/tmp"
    };

    size_t i;
    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (wasm_ensure_directory(paths[i]) < 0) {
            return -1;
        }
    }

    return 0;
}

static int wasm_assign_string(char **target, const char *value)
{
    char *copy;

    if (!value) {
        return -1;
    }

    copy = strdup(value);
    if (!copy) {
        return -1;
    }

    free(*target);
    *target = copy;
    return 0;
}

static int wasm_setup_program_paths(void)
{
    /* Use platform API to get default paths */
    const char *program_path = psetting_platform_get_virtual_path("program");
    const char *autoload_path = psetting_platform_get_virtual_path("autoload");

    if (!program_path || !autoload_path) {
        /* Initialize defaults if not set */
        psetting_platform_init_paths(&configAPI);
        program_path = psetting_platform_get_virtual_path("program");
        autoload_path = psetting_platform_get_virtual_path("autoload");
    }

    if (wasm_assign_string(&cfProgramPath, program_path) < 0) {
        return -1;
    }

    if (wasm_assign_string(&cfProgramPathAutoload, autoload_path) < 0) {
        return -1;
    }

    return 0;
}

static int wasm_configure_config_paths(void)
{
    /* Use platform API to configure all paths */
    return psetting_platform_init_paths(&configAPI);
}

static const char wasm_mdbsigv2[60] =
    "Cubic Player Module Information Data Base II\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01";
static const char wasm_dirdbsigv2[60] =
    "Cubic Player Directory Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01";
static const char wasm_adbMetaTag[16] = "OCPArchiveMeta\x1b\x00";
static const char wasm_musicbrainzsigv1[64] =
    "Cubic Player MusicBrainz Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

static int file_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0;
}

static int copy_file_if_present(const char *src, const char *dst)
{
    int src_fd;
    int dst_fd;
    int result = -1;

    src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        if (errno == ENOENT) {
            return 1;
        }
        fprintf(stderr, "copy_file_if_present: open(%s) failed: %s\n", src, strerror(errno));
        return -1;
    }

    dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (dst_fd < 0) {
        fprintf(stderr, "copy_file_if_present: open(%s) failed: %s\n", dst, strerror(errno));
        close(src_fd);
        return -1;
    }

    while (1) {
        char buffer[4096];
        ssize_t r = read(src_fd, buffer, sizeof(buffer));
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "copy_file_if_present: read(%s) failed: %s\n", src, strerror(errno));
            goto copy_cleanup;
        }
        if (r == 0) {
            break;
        }
        ssize_t offset = 0;
        while (offset < r) {
            ssize_t w = write(dst_fd, buffer + offset, (size_t)(r - offset));
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                fprintf(stderr, "copy_file_if_present: write(%s) failed: %s\n", dst, strerror(errno));
                goto copy_cleanup;
            }
            offset += w;
        }
    }

    result = 0;

copy_cleanup:
    close(src_fd);
    if (close(dst_fd) < 0 && result == 0) {
        fprintf(stderr, "copy_file_if_present: close(%s) failed: %s\n", dst, strerror(errno));
        result = -1;
    }
    if (result < 0) {
        unlink(dst);
    }
    return result;
}

static int wasm_seed_default_config(void)
{
    char dest[PATH_MAX];
    int rc;

    if (!configAPI.ConfigHomePath) {
        return -1;
    }
    if (snprintf(dest, sizeof(dest), "%socp.ini", configAPI.ConfigHomePath) >= (int)sizeof(dest)) {
        fprintf(stderr, "WASM: config path truncated\n");
        return -1;
    }
    if (file_exists(dest)) {
        return 0;
    }
    rc = copy_file_if_present("/assets/ocp.ini", dest);
    if (rc == 1) {
        fprintf(stderr, "WASM: packaged /assets/ocp.ini is missing\n");
        return -1;
    }
    return rc;
}

static int wasm_read_stored_config(void)
{
    int argc = stored_argc;
    char **argv = stored_argv;
    static char *fallback_argv[] = { "ocp", NULL };

    if (argc <= 0 || !argv) {
        argc = 1;
        argv = fallback_argv;
    }
    return cfGetConfig(argc, argv);
}

static void wasm_apply_browser_device_defaults(void)
{
    const char *sound = (configAPI.SoundSec && configAPI.SoundSec[0]) ? configAPI.SoundSec : "sound";
    const char *players = cfGetProfileString(sound, "playerdevices", NULL);
    const char *waves = cfGetProfileString(sound, "wavetabledevices", NULL);
    const char *screen = (configAPI.ScreenSec && configAPI.ScreenSec[0]) ? configAPI.ScreenSec : "screen";

    /* A saved list that already names the browser driver is left alone.
     * A desktop ini, or a missing key, is pointed at the drivers this build has. */
    if (!players || !strstr(players, "devpSDL2")) {
        cfSetProfileString(sound, "playerdevices", "devpSDL2 devpNone");
    }
    if (!waves || !strstr(waves, "devwMix")) {
        cfSetProfileString(sound, "wavetabledevices", "devwMix devwMixF devwNone");
    }
    if (!cfGetProfileString(sound, "sdl_buffer_ms", NULL)) {
        cfSetProfileInt(sound, "sdl_buffer_ms", 50, 10);
    }
    if (!cfGetProfileString(screen, "analyser", NULL)) {
        cfSetProfileBool(screen, "analyser", 1);
    }
}

static int create_cpmodnfo(const char *path)
{
    struct __attribute__((packed)) bootstrap_mdbheader {
        char sig[60];
        uint32_t entries;
    } header;
    const uint32_t initial_entries = 64;
    uint8_t empty_record[64] = {0};
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        fprintf(stderr, "create_cpmodnfo: open(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }
    memcpy(header.sig, wasm_mdbsigv2, sizeof(header.sig));
    header.entries = initial_entries;
    if (write(fd, &header, sizeof(header)) != (ssize_t)sizeof(header)) {
        fprintf(stderr, "create_cpmodnfo: failed to write header to %s\n", path);
        close(fd);
        unlink(path);
        return -1;
    }
    for (uint32_t i = 1; i < initial_entries; ++i) {
        if (write(fd, empty_record, sizeof(empty_record)) != (ssize_t)sizeof(empty_record)) {
            fprintf(stderr, "create_cpmodnfo: failed to prime record %u in %s\n", i, path);
            close(fd);
            unlink(path);
            return -1;
        }
    }
    close(fd);
    return 0;
}

static int create_cpdir(const char *path)
{
    struct __attribute__((packed)) bootstrap_dirdbheader {
        char sig[60];
        uint32_t entries;
    } header;
    const uint32_t initial_entries = 512;
    uint16_t zero_len = 0;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        fprintf(stderr, "create_cpdir: open(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }
    memcpy(header.sig, wasm_dirdbsigv2, sizeof(header.sig));
    header.entries = initial_entries;
    if (write(fd, &header, sizeof(header)) != (ssize_t)sizeof(header)) {
        fprintf(stderr, "create_cpdir: failed to write header to %s\n", path);
        close(fd);
        unlink(path);
        return -1;
    }
    for (uint32_t i = 0; i < initial_entries; ++i) {
        if (write(fd, &zero_len, sizeof(zero_len)) != (ssize_t)sizeof(zero_len)) {
            fprintf(stderr, "create_cpdir: failed to prime entry %u in %s\n", i, path);
            close(fd);
            unlink(path);
            return -1;
        }
    }
    close(fd);
    return 0;
}

static int create_adbmeta(const char *path)
{
    struct __attribute__((packed)) bootstrap_adbmeta_header {
        char sig[16];
        uint32_t entries;
    } header;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        fprintf(stderr, "create_adbmeta: open(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }
    memcpy(header.sig, wasm_adbMetaTag, sizeof(header.sig));
    header.entries = 0;
    if (write(fd, &header, sizeof(header)) != (ssize_t)sizeof(header)) {
        fprintf(stderr, "create_adbmeta: failed to write header to %s\n", path);
        close(fd);
        unlink(path);
        return -1;
    }
    close(fd);
    return 0;
}

static int create_musicbrainz(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        fprintf(stderr, "create_musicbrainz: open(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }
    if (write(fd, wasm_musicbrainzsigv1, sizeof(wasm_musicbrainzsigv1)) != (ssize_t)sizeof(wasm_musicbrainzsigv1)) {
        fprintf(stderr, "create_musicbrainz: failed to write header to %s\n", path);
        close(fd);
        unlink(path);
        return -1;
    }
    close(fd);
    return 0;
}

static int ensure_database_file(const char *name,
                                int (*create_fn)(const char *),
                                int attempt_copy_from_assets)
{
    char dest[PATH_MAX];
    if (!name || !create_fn) {
        return -1;
    }
    if (snprintf(dest, sizeof(dest), "%s%s", configAPI.DataHomePath, name) >= (int)sizeof(dest)) {
        fprintf(stderr, "ensure_database_file: destination truncated for %s\n", name);
        return -1;
    }

    if (file_exists(dest)) {
        return 0;
    }

    if (attempt_copy_from_assets) {
        char src[PATH_MAX];
        if (snprintf(src, sizeof(src), "%s%s", configAPI.DataPath, name) >= (int)sizeof(src)) {
            fprintf(stderr, "ensure_database_file: source truncated for %s\n", name);
            return -1;
        }
        int copy_res = copy_file_if_present(src, dest);
        if (copy_res == 0) {
            return 0;
        }
        if (copy_res < 0) {
            return -1;
        }
    }

    return create_fn(dest);
}

static int wasm_bootstrap_database_files(void)
{
    if (ensure_database_file("CPMODNFO.DAT", create_cpmodnfo, 1) < 0) {
        return -1;
    }
    if (ensure_database_file("CPDIRDB.DAT", create_cpdir, 1) < 0) {
        return -1;
    }
    if (ensure_database_file("CPARCMETA.DAT", create_adbmeta, 1) < 0) {
        return -1;
    }
    if (ensure_database_file("CPMUSBRN.DAT", create_musicbrainz, 1) < 0) {
        return -1;
    }
    return 0;
}

// Wrapper for fsRegisterExt that also tracks extensions in WASM-side registry
static void wasm_fsRegisterExt_wrapper(const char *ext)
{
    extern void fsRegisterExt(const char *ext);
    size_t i;

    // Call the original function
    fsRegisterExt(ext);

    // Check if already registered in our WASM list
    for (i = 0; i < wasm_registered_extensions_count; i++) {
        if (wasm_registered_extensions[i] && strcasecmp(wasm_registered_extensions[i], ext) == 0) {
            return; // Already tracked
        }
    }

    // Add to our WASM-side list
    if (wasm_registered_extensions_count >= wasm_registered_extensions_capacity) {
        size_t new_capacity = wasm_registered_extensions_capacity == 0 ? 16 : wasm_registered_extensions_capacity * 2;
        char **new_array = realloc(wasm_registered_extensions, new_capacity * sizeof(char*));
        if (!new_array) {
            fprintf(stderr, "WASM: Failed to allocate memory for extension registry\n");
            return;
        }
        wasm_registered_extensions = new_array;
        wasm_registered_extensions_capacity = new_capacity;
    }

    wasm_registered_extensions[wasm_registered_extensions_count] = strdup(ext);
    if (!wasm_registered_extensions[wasm_registered_extensions_count]) {
        fprintf(stderr, "WASM: Failed to duplicate extension string\n");
        return;
    }
    wasm_registered_extensions_count++;
}

static void wasm_configure_plugin_interfaces(void)
{
    extern void mdbRegisterReadInfo(struct mdbreadinforegstruct *r);
    extern void mdbUnregisterReadInfo(struct mdbreadinforegstruct *r);
    extern void debug_fsTypeRegister(struct moduletype modtype, const char **description, const char *interfacename, const struct cpifaceplayerstruct *cp);
    extern void fsTypeUnregister(struct moduletype modtype);
    extern int mcpRegisterPostProcFP(const struct PostProcFPRegStruct *plugin);
    extern int mcpRegisterPostProcInteger(const struct PostProcIntegerRegStruct *plugin);
    extern void mcpUnregisterPostProcFP(const struct PostProcFPRegStruct *plugin);
    extern void mcpUnregisterPostProcInteger(const struct PostProcIntegerRegStruct *plugin);
    extern void mcpRegisterDriver(const struct mcpDriver_t *driver);
    extern void mcpUnregisterDriver(const struct mcpDriver_t *driver);
    extern void plrUnregisterDriver(const struct plrDriver_t *driver);

    wasm_plugin_api.mdbRegisterReadInfo = mdbRegisterReadInfo;
    wasm_plugin_api.fsTypeRegister = debug_fsTypeRegister;
    wasm_plugin_api.fsRegisterExt = wasm_fsRegisterExt_wrapper;
    wasm_plugin_api.plrRegisterDriver = plrRegisterDriver;
    wasm_plugin_api.mcpRegisterDriver = mcpRegisterDriver;
    wasm_plugin_api.mcpRegisterPostProcFP = mcpRegisterPostProcFP;
    wasm_plugin_api.mcpRegisterPostProcInteger = mcpRegisterPostProcInteger;
    wasm_plugin_api.configAPI = &configAPI;
    wasm_plugin_api.dirdb = &dirdbAPI;
    wasm_plugin_api.dev_file_create = dev_file_create;
    wasm_plugin_api.filesystem_setup_register_file = filesystem_setup_register_file;
    wasm_plugin_api.dmSetup = dmSetup;

    wasm_plugin_close_api.mdbUnregisterReadInfo = mdbUnregisterReadInfo;
    wasm_plugin_close_api.fsTypeUnregister = fsTypeUnregister;
    wasm_plugin_close_api.plrUnregisterDriver = plrUnregisterDriver;
    wasm_plugin_close_api.mcpUnregisterDriver = mcpUnregisterDriver;
    wasm_plugin_close_api.mcpUnregisterPostProcFP = mcpUnregisterPostProcFP;
    wasm_plugin_close_api.mcpUnregisterPostProcInteger = mcpUnregisterPostProcInteger;
    wasm_plugin_close_api.filesystem_setup_unregister_file = filesystem_setup_unregister_file;
}

// WASM state structure
typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_AudioDeviceID audio_device;
    int is_initialized;
    int is_running;
    char current_file[256];
} wasm_state_t;

static wasm_state_t g_wasm_state = {0};

/* Clear pending file and reset module info
 * Ensures proper cleanup and prevents resource leaks
 */
static void wasm_runtime_clear_pending(void)
{
	if (g_runtime.pending_filehandle) {
		g_runtime.pending_filehandle->unref(g_runtime.pending_filehandle);
		g_runtime.pending_filehandle = NULL;
	}
	memset(&g_runtime.pending_module_info, 0, sizeof(g_runtime.pending_module_info));
}

void wasm_enable_file_selector(void)
{
	wasm_runtime_clear_pending();
	g_runtime.mode = WASM_MODE_FILESEL;
	g_wasm_state.is_running = 0;
	g_wasm_state.is_initialized = 0;
}

/* Enable file selector overlay during playback
 * Unlike wasm_enable_file_selector(), this keeps playback running
 * and allows the user to select a new file or return to playback
 */
void wasm_enable_file_selector_overlay(void)
{
	g_runtime.mode = WASM_MODE_PLAYBACK_WITH_FILESEL;
	/* Note: we do NOT call wasm_runtime_clear_pending() or change is_running/is_initialized
	 * because we want to keep the current playback state intact */
}

// Note: Audio system is now handled entirely by OCP's MCP system



// Include the interface system after our declarations
#include "../stuff/poutput-sdl2.h"
#include "wasm-interface-main.c"

// Main OCP function implementation for WASM
static int wasm_ocp_main(int argc, char *argv[]) {
    // Initialize the interface system
	if (wasm_interface_init() != 0) {
		fprintf(stderr, "WASM: Failed to initialize interface system\n");
		return -1;
	}

    // Start the interface main loop
    wasm_interface_start();

    g_wasm_state.window = NULL;
    g_wasm_state.renderer = NULL;

    // Initialize display system - this sets Console.VidType = vidModern
	if (plDisplayInit() < 0) {
		fprintf(stderr, "WASM: Failed to initialize OCP display system\n");
		return -1;
	}
    cpiSetTextMode(0);

    // Now call cpiface LateInit - this will call cpiVerifyDefModes()
    // At this point Console.VidType is properly set, so graph/scope modes will pass verification
	if (cpiface_dllextinfo.LateInit && cpiface_dllextinfo.LateInit(&wasm_plugin_api) < 0) {
		fprintf(stderr, "WASM: Failed to late-initialize cpiface\n");
		return -1;
	}

    // Initialize file selector late init
    extern int fsLateInit(const struct configAPI_t *configAPI);
    extern struct configAPI_t configAPI;
	if (fsLateInit(&configAPI) < 0) {
		fprintf(stderr, "WASM: Failed to late-initialize file selector\n");
		return -1;
	}

    // Set up dmCurDrive to point to the file:// drive and navigate to /music
    extern struct dmDrive *dmFile;
    extern struct dmDrive *dmCurDrive;
	if (dmFile) {
		dmCurDrive = dmFile;
		const char *basedir_name = NULL;
		if (dmFile->basedir && dirdbAPI.GetName_internalstr) {
			dirdbAPI.GetName_internalstr(dmFile->basedir->dirdb_ref, &basedir_name);
		}

		// Navigate to /music directory
		extern const struct dirdbAPI_t dirdbAPI;
		uint32_t music_ref = dirdbAPI.ResolvePathWithBaseAndRef(dmFile->basedir->dirdb_ref, "music", 0, dirdb_use_dir);
		if (music_ref != DIRDB_NOPARENT) {
			struct ocpdir_t *music_dir = dmFile->basedir->readdir_dir(dmFile->basedir, music_ref);
			if (music_dir) {
				if (dmCurDrive->cwd) {
					dmCurDrive->cwd->unref(dmCurDrive->cwd);
				}
				dmCurDrive->cwd = music_dir;
			}
			dirdbAPI.Unref(music_ref, dirdb_use_dir);
		}
	} else {
		fprintf(stderr, "WASM: Warning - dmFile not initialized\n");
	}

    // Show startup banner before entering the selector loop.
    displaystr(0, 0, 0x07, "OpenCubicPlayer WASM - File Selector", 37);
    displaystr(1, 0, 0x0F, "Loading sample files from /music...", 36);

	// Prepare file selector
	wasm_enable_file_selector();

    wasm_ocp_main_called = 1;  // Signal that initialization is complete

    return 0;
}

// Register our main function with the OCP system
static struct mainstruct wasm_main_struct = {
    .main = wasm_ocp_main
};

extern void wasm_main_loop(void);

EMSCRIPTEN_KEEPALIVE
int wasm_startup_is_complete(void)
{
    return wasm_startup_completed;
}

EMSCRIPTEN_KEEPALIVE
int wasm_start_ocp(void)
{
    if (wasm_runtime_started) {
        return 0;
    }

    /* Device setup reads ocp.ini. That read finishes in the IDBFS callback,
     * which the page waits for before calling this. Starting earlier would
     * apply defaults and ignore the saved file. */
    if (!wasm_startup_completed) {
        fprintf(stderr, "WASM: wasm_start_ocp called before ocp.ini was read\n");
        wasm_report_page_error("OpenCubicPlayer started before ocp.ini was read.");
        return -1;
    }

	if (wasm_setup_program_paths() < 0) {
		fprintf(stderr, "WASM: Failed to set up program paths\n");
		wasm_report_page_error("OpenCubicPlayer could not set up its program paths.");
		return -1;
	}

    // Configure plugin API first (before any PluginInit calls)
    framelock_init();
    lnkInit();
    memset(&wasm_plugin_api, 0, sizeof(wasm_plugin_api));
    memset(&wasm_plugin_close_api, 0, sizeof(wasm_plugin_close_api));
    wasm_configure_plugin_interfaces();

    extern void wasm_init_config_sections(void);
    wasm_init_config_sections();

    /* Fill browser device keys only when the loaded ini does not already name them. */
    wasm_apply_browser_device_defaults();

    // Initialize core modules BEFORE loading dynamic plugins
    // These are statically linked and not part of the plugin system
	if (cpiface_dllextinfo.Init && cpiface_dllextinfo.Init(&configAPI) < 0) {
		fprintf(stderr, "WASM: Failed to initialize cpiface\n");
		wasm_report_page_error("OpenCubicPlayer could not initialize the player interface.");
		return -1;
	}

	if (cphlpif_dllextinfo.Init && cphlpif_dllextinfo.Init(&configAPI) < 0) {
		fprintf(stderr, "WASM: Warning - failed to initialize help system\n");
	}

	// Call PreInit for statically linked modules to set up config-based driver slots
	if (deviplay_dllextinfo.PreInit && deviplay_dllextinfo.PreInit(&configAPI) < 0) {
		fprintf(stderr, "WASM: Failed to run deviplay PreInit\n");
		wasm_report_page_error("OpenCubicPlayer could not initialize the playback device list.");
		return -1;
	}

	if (deviwave_dllextinfo.PreInit && deviwave_dllextinfo.PreInit(&configAPI) < 0) {
		fprintf(stderr, "WASM: Failed to run deviwave PreInit\n");
		wasm_report_page_error("OpenCubicPlayer could not initialize the wavetable device list.");
		return -1;
	}

	if (deviplay_dllextinfo.PluginInit && deviplay_dllextinfo.PluginInit(&wasm_plugin_api) < 0) {
		fprintf(stderr, "WASM: Failed to initialize deviplay\n");
		wasm_report_page_error("OpenCubicPlayer could not start the playback devices.");
		return -1;
	}

	if (deviwave_dllextinfo.PluginInit && deviwave_dllextinfo.PluginInit(&wasm_plugin_api) < 0) {
		fprintf(stderr, "WASM: Failed to initialize deviwave\n");
		wasm_report_page_error("OpenCubicPlayer could not start the wavetable devices.");
		return -1;
	}

	// Now load dynamic plugins (devices, effects, playback formats, and filesystem drivers)
	wasm_report_loading_status("Loading plugins from autoload directory...");
	if (lnkLinkDir(cfProgramPathAutoload) < 0) {
		fprintf(stderr, "WASM: Failed to autoload plugins from %s\n", cfProgramPathAutoload);
		wasm_report_page_error("OpenCubicPlayer could not load plugins from the autoload directory.");
		return -1;
	}

	wasm_report_loading_status("Initializing plugins (PreInit/Init)...");
	if (lnkInitAll()) {
		fprintf(stderr, "WASM: lnkInitAll failed\n");
		wasm_report_page_error("A plugin failed during initialization.");
		return -1;
	}

	wasm_report_loading_status("Registering plugin features...");
	if (lnkPluginInitAll(&wasm_plugin_api)) {
		fprintf(stderr, "WASM: lnkPluginInitAll failed\n");
		wasm_report_page_error("A plugin failed while registering its features.");
		return -1;
	}

    wasm_plugins_initialized = 1;

    // Note: cpiface LateInit is deferred until after display is initialized in wasm_ocp_main()
    // This ensures Console.VidType is set before cpiVerifyDefModes() runs

	if (cphelper_dllextinfo.PluginInit && cphelper_dllextinfo.PluginInit(&wasm_plugin_api) < 0) {
		fprintf(stderr, "WASM: Warning - failed to initialize help loader plugin\n");
	}

	if (deviplay_dllextinfo.LateInit && deviplay_dllextinfo.LateInit(&wasm_plugin_api) < 0) {
		fprintf(stderr, "WASM: Failed to run deviplay LateInit\n");
		wasm_report_page_error("OpenCubicPlayer could not finish playback device setup.");
		return -1;
	}

	if (deviwave_dllextinfo.LateInit && deviwave_dllextinfo.LateInit(&wasm_plugin_api) < 0) {
		fprintf(stderr, "WASM: Failed to run deviwave LateInit\n");
		wasm_report_page_error("OpenCubicPlayer could not finish wavetable device setup.");
		return -1;
	}

    ocpmain = &wasm_main_struct;

    wasm_runtime_started = 1;

    // Call wasm_ocp_main - this now prepares the file selector for stepper-based updates
	if (wasm_ocp_main(stored_argc, stored_argv) < 0) {
		fprintf(stderr, "WASM: wasm_ocp_main failed\n");
		wasm_report_page_error("OpenCubicPlayer failed while opening the file selector.");
		return -1;
	}

	// Set up the main loop to drive the stepper-based interfaces
    // IMPORTANT: Use setInterval from JavaScript instead of emscripten_set_main_loop
    // to prevent background tab throttling that would pause audio
    // emscripten_set_main_loop uses requestAnimationFrame which gets throttled to 1fps in background tabs
    EM_ASM({
        // Use setInterval instead of requestAnimationFrame to keep audio playing in background
        // 60 FPS = ~16.67ms interval
        Module._mainLoopInterval = setInterval(function() {
            try {
                Module._wasm_main_loop();
            } catch (err) {
                if (err === 'unwind' || (err && err.message === 'unwind')) {
                    return;
                }
                // A trap leaves the player in an unknown state. Stop the loop
                // and tell the visitor instead of throwing on every frame.
                clearInterval(Module._mainLoopInterval);
                console.error('wasm_main_loop', err);
                if (typeof window !== 'undefined' && typeof window.ocpShowFatalError === 'function') {
                    window.ocpShowFatalError('OpenCubicPlayer crashed. Reload the page to restart it.');
                }
            }
        }, 16);
    });

    return 0;
}

/* Drive the file selector state machine
 * Called from main loop when in FILESEL or FILESEL_SELECTED modes
 */
EMSCRIPTEN_KEEPALIVE
static void wasm_drive_file_selector(void)
{
    int fs_result;

    /* If we're already in FILESEL_SELECTED mode, skip the file selector
     * (file was already selected, e.g., from overlay mode) */
    if (g_runtime.mode == WASM_MODE_FILESEL_SELECTED) {
        goto load_file;
    }

    fs_result = fsFileSelect();

    /* Handle file selector errors or cancellation */
	/* A browser tab has nowhere to quit to. Reopen the selector on quit or
	 * error. A failure on three frames in a row is not a quit, so stop with
	 * a visible message. */
	static int consecutive_failures = 0;
	if (fs_result < 0) {
		fprintf(stderr, "WASM: File selector returned error or user quit (%d)\n", fs_result);
		if (++consecutive_failures >= 3) {
			wasm_runtime_clear_pending();
			g_runtime.mode = WASM_MODE_ERROR;
			wasm_report_page_error("The file selector failed repeatedly. Reload the page to restart it.");
			return;
		}
		wasm_enable_file_selector();
		return;
	}
	consecutive_failures = 0;

    /* Check if file selector exited without selection */
	if (fs_result == 0) {
		if (!fsFileSelectIsActive()) {
			wasm_enable_file_selector();
		}
		return;
	}

    /* File selected - retrieve it if we haven't already */
    if (g_runtime.mode == WASM_MODE_FILESEL) {
		if (fsGetNextFile(&g_runtime.pending_module_info, &g_runtime.pending_filehandle) && g_runtime.pending_filehandle) {
			g_runtime.mode = WASM_MODE_FILESEL_SELECTED;
		} else {
			fprintf(stderr, "WASM: fsGetNextFile failed to retrieve selected file\n");
			wasm_runtime_clear_pending();
			return;
		}
    }

load_file:
    /* Try to load the selected file */
    if (g_runtime.mode == WASM_MODE_FILESEL_SELECTED && g_runtime.pending_filehandle) {
        const struct interfacestruct *interface = NULL;
        const struct cpifaceplayerstruct *player = NULL;
        plFindInterface(g_runtime.pending_module_info.modtype, &interface, &player);

		if (interface) {
			if (wasm_interface_load_from_handle(&g_runtime.pending_module_info, g_runtime.pending_filehandle, interface, player) == 0) {
				// Check if a player was actually loaded successfully
				// (plmpOpenFile returns success but sets curplayer=NULL on failure)
				extern int wasm_cpiface_has_valid_player(void);
				int has_valid_player = wasm_cpiface_has_valid_player();

				if (player && has_valid_player) {
					g_wasm_state.is_running = 1;
					g_wasm_state.is_initialized = 1;
					g_runtime.mode = WASM_MODE_PLAYBACK;
				} else if (player && !has_valid_player) {
					// Player was requested but failed to load - show error dialog
					// Use VIRTUAL_INTERFACE mode to display the error without audio
					g_runtime.mode = WASM_MODE_VIRTUAL_INTERFACE;
				} else {
					g_runtime.mode = WASM_MODE_VIRTUAL_INTERFACE;
				}
				wasm_runtime_clear_pending();
			} else {
				fprintf(stderr, "WASM: Interface initialization failed, returning to file selector\n");
				wasm_runtime_clear_pending();
				g_runtime.mode = WASM_MODE_FILESEL;
			}
		} else {
			fprintf(stderr, "WASM: No interface found for file type 0x%08x\n", g_runtime.pending_module_info.modtype.integer.i);
			wasm_runtime_clear_pending();
			g_runtime.mode = WASM_MODE_FILESEL;
		}
    }
}

/* Drive the file selector while playback continues
 * Called from main loop when in PLAYBACK_WITH_FILESEL mode
 * This allows the user to select a new file while audio continues playing
 */
EMSCRIPTEN_KEEPALIVE
static void wasm_drive_file_selector_with_playback(void)
{
    int fs_result = fsFileSelect();

    /* Still selecting - keep both file selector and playback active */
    if (fs_result == 0 && fsFileSelectIsActive()) {
        return;
    }

    /* User selected a file */
    if (fs_result == 1) {
        struct moduleinfostruct new_info;
        struct ocpfilehandle_t *new_file = NULL;

		if (fsGetNextFile(&new_info, &new_file) && new_file) {
			/* Close the current interface since user selected a new file */
			extern void wasm_close_interface(void);
			wasm_close_interface();

			/* Store the new file for loading */
			g_runtime.pending_module_info = new_info;
			g_runtime.pending_filehandle = new_file;
			g_runtime.mode = WASM_MODE_FILESEL_SELECTED;
		} else {
			/* Selection failed - return to playback */
			fprintf(stderr, "WASM: Failed to get selected file, returning to playback\n");
			g_runtime.mode = WASM_MODE_PLAYBACK;
		}
    } else {
		/* User cancelled (fs_result == 0 && !active) or error (fs_result < 0) */
		g_runtime.mode = WASM_MODE_PLAYBACK;

        /* Playback state (g_wasm_state) remains unchanged, so music keeps playing */
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_main_loop(void) {
    /* If wasm_ocp_main hasn't been called yet, just return */
    if (!wasm_ocp_main_called) {
        return;
    }

    /* Periodic IDBFS maintenance - check for deferred syncs */
    extern void idbfs_tick(void);
    idbfs_tick();

    /* Handle file selector modes */
    if (g_runtime.mode == WASM_MODE_FILESEL || g_runtime.mode == WASM_MODE_FILESEL_SELECTED) {
        wasm_drive_file_selector();
        return;
    }

    /* Handle file selector during playback - audio continues in background */
    if (g_runtime.mode == WASM_MODE_PLAYBACK_WITH_FILESEL) {
        wasm_drive_file_selector_with_playback();
        return;
    }

    int interface_only = (g_runtime.mode == WASM_MODE_VIRTUAL_INTERFACE);

    /* If not in playback or virtual interface mode, nothing to do */
    if (!interface_only) {
        if (g_runtime.mode != WASM_MODE_PLAYBACK) {
            return;
        }
    }

    /* Additional check: verify playback is actually running */
    if (!interface_only) {
        if (!g_wasm_state.is_initialized || !g_wasm_state.is_running) {
            return;
        }
    }

    // Drive any registered idle hooks (cpiface idle, etc.)
    if (!interface_only) {
        wasm_audio_tick();
    }

    /* Quit only. Keys, text, and resize stay queued for ekbhit_sdl2dummy. */
    SDL_PumpEvents();
    {
        SDL_Event event;
        while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUIT, SDL_QUIT) > 0) {
            if (!interface_only) {
                g_wasm_state.is_running = 0;
            }
        }
    }

    // Drive the original interface each frame so the classic UI updates on the canvas
    wasm_interface_main_loop();

    // Process keyboard input and interface events (for cpiface mode only)
    if (!interface_only && cpifaceSessionAPI.Public.ProcessKey) {
        // Poll for pending key events and process them
        // This drives the OCP interface system
        cpifaceSessionAPI.Public.ProcessKey(&cpifaceSessionAPI.Public, 0);
    }

    // Call frame timing
    framelock();

    // In WASM, SDL2 handles the display updates automatically

    // Drive the audio system for music playback
    // We need to call both MCP and PLR Idle functions to generate audio data
    extern int g_is_playing;
    extern struct cpifaceSessionPrivate_t cpifaceSessionAPI;

    // Only drive audio when actually playing (after play button is clicked)
    if (!interface_only && g_is_playing) {
        // Rate-limited audio generation to prevent buffer overflow
        // At 48kHz with 1024 samples per chunk, we need new data every ~21ms
        // At 60 FPS main loop, that's about every 1.3 frames
        static uint32_t last_audio_time = 0;
        uint32_t current_time = SDL_GetTicks();

        // Only generate audio if enough time has passed OR if buffer is getting low
        int should_generate_audio = (current_time - last_audio_time >= 20);

        if (!should_generate_audio) {
            extern const struct plrDevAPI_t *plrDevAPI;
            if (plrDevAPI && plrDevAPI->PeekBuffer) {
                void *buf1, *buf2;
                unsigned int buf1length, buf2length;
                plrDevAPI->PeekBuffer(&buf1, &buf1length, &buf2, &buf2length);
                if ((buf1length + buf2length) < 1024) {
                    should_generate_audio = 1;
                }
            }
        }

        if (should_generate_audio) {
            last_audio_time = current_time;

            // First call MCP Idle to process the MOD and generate samples
            if (cpifaceSessionAPI.Public.mcpDevAPI && cpifaceSessionAPI.Public.mcpDevAPI->Idle) {
                cpifaceSessionAPI.Public.mcpDevAPI->Idle(&cpifaceSessionAPI.Public);
            }

            // Then call PLR Idle to manage the output buffer
            extern const struct plrDevAPI_t *plrDevAPI;
            if (plrDevAPI && plrDevAPI->Idle) {
                plrDevAPI->Idle();
            }

            // Note: Manual audio generation removed - now handled by real mcpDevAPI->Idle from devwmix.c
        }
    }
}

static int wasm_finish_startup_sequence(void)
{
	if (wasm_startup_completed) {
		return 0;
	}

	// Paths, directories, and a readable ocp.ini must exist before cfGetConfig.
	if (wasm_configure_config_paths() < 0) {
		fprintf(stderr, "WASM: Failed to configure config paths\n");
		wasm_report_page_error("OpenCubicPlayer could not set up its config directory.");
		return -1;
	}

	/* Ensure persistent directories exist under the IDBFS mount */
	if (wasm_ensure_directory(WASM_CONFIG_DIR) < 0) {
		fprintf(stderr, "WASM: Failed to create config directory %s\n", WASM_CONFIG_DIR);
		wasm_report_page_error("OpenCubicPlayer could not create its config directory.");
		return -1;
	}
	if (wasm_ensure_directory(WASM_DATAHOME_DIR) < 0) {
		fprintf(stderr, "WASM: Failed to create data directory %s\n", WASM_DATAHOME_DIR);
		wasm_report_page_error("OpenCubicPlayer could not create its data directory.");
		return -1;
	}

	if (wasm_seed_default_config() < 0) {
		wasm_report_page_error("OpenCubicPlayer could not create its default ocp.ini.");
		return -1;
	}

	if (wasm_read_stored_config() < 0) {
		fprintf(stderr, "WASM: Failed to read ocp.ini\n");
		wasm_report_page_error("OpenCubicPlayer could not read ocp.ini.");
		return -1;
	}

	extern void wasm_init_config_sections(void);
	wasm_init_config_sections();

	// Install WASM config wrapper to sync IDBFS after config changes
	extern void wasm_config_wrapper_init(void);
	wasm_config_wrapper_init();

	if (wasm_bootstrap_database_files() < 0) {
		fprintf(stderr, "WASM: Failed to bootstrap database files\n");
		wasm_report_page_error("OpenCubicPlayer could not create its database files.");
		return -1;
	}

	// Initialize console system
	if (conInit() < 0) {
		fprintf(stderr, "WASM: Failed to initialize console system\n");
		wasm_report_page_error("OpenCubicPlayer could not initialize the console.");
		return -1;
	}

	// Initialize error system
	if (errInit() < 0) {
		fprintf(stderr, "WASM: Failed to initialize error system\n");
		wasm_report_page_error("OpenCubicPlayer could not initialize its error handler.");
		return -1;
	}

	// Initialize directory database (required for file handling)
	extern int dirdbInit(const struct configAPI_t *configAPI);
	if (dirdbInit(&configAPI) < 0) {
		fprintf(stderr, "WASM: Failed to initialize directory database\n");
		wasm_report_page_error("OpenCubicPlayer could not open its directory database.");
		return -1;
	}

	// Initialize file selector early - before display system
	extern int fsPreInit(const struct configAPI_t *configAPI);
	extern int fsInit(void);
	if (!fsPreInit(&configAPI)) {
		fprintf(stderr, "WASM: Failed to pre-initialize file selector\n");
		wasm_report_page_error("OpenCubicPlayer could not prepare the file selector.");
		return -1;
	}
	if (!fsInit()) {
		fprintf(stderr, "WASM: Failed to initialize file selector\n");
		wasm_report_page_error("OpenCubicPlayer could not start the file selector.");
		return -1;
	}

	// Apply sample file metadata from manifest AFTER mdb is initialized
	// but BEFORE file selector displays its list
	extern int wasm_apply_sample_metadata_from_manifest(void);
	wasm_apply_sample_metadata_from_manifest();

	wasm_startup_completed = 1;
	return 0;
}

static void wasm_on_initial_sync(int success, void *user_data)
{
	(void)user_data;

	if (!success) {
		fprintf(stderr, "WASM: Warning - IDBFS populate timeout or failure, continuing\n");
		wasm_report_page_notice("Saved settings could not be loaded from this browser. Continuing with defaults.");
	}

	if (wasm_finish_startup_sequence() < 0) {
		fprintf(stderr, "WASM: Startup failed after IDBFS sync\n");
		emscripten_force_exit(1);
	}
}

// Main entry point for WASM
int main(int argc, char *argv[]) {
    stored_argc = argc;
    stored_argv = argv;

	if (wasm_prepare_directories() < 0) {
		fprintf(stderr, "WASM: Failed to prepare base directories\n");
		wasm_report_page_error("OpenCubicPlayer could not prepare its directories.");
		return -1;
	}

	// Initialize IDBFS persistence (mounts IndexedDB at /home/web_user/.ocp)
	// This must happen AFTER parent directories are created but BEFORE config init
	if (idbfs_init() < 0) {
		fprintf(stderr, "WASM: Failed to initialize IDBFS persistence\n");
		wasm_report_page_error("IndexedDB is unavailable, so settings cannot be stored. Private browsing often blocks it.");
		return -1;
	}

	// Wait for initial IDBFS populate to complete (loads saved config/data)
	fprintf(stderr, "WASM: Waiting for IDBFS to populate from IndexedDB...\n");
	if (idbfs_wait_for_sync(10000, wasm_on_initial_sync, NULL) < 0) {
		fprintf(stderr, "WASM: Warning - unable to queue IDBFS wait, continuing immediately\n");
		wasm_on_initial_sync(0, NULL);
	}

	return 0;
}

// Playback is driven by the on-canvas interface.
// The page calls idbfs_flush from pagehide so the last config write is stored.
