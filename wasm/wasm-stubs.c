/* OpenCP Module Player - WASM Stubs
 * Copyright (c) 2025-2026 Christophe Thibault - WASM port stubs for original DOS interface
 *
 * This file contains stubs for functionality that is not available or needed in WASM
 * These stubs allow us to use the original DOS interface files without modification
 */

#include "config.h"
#include "../types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <emscripten.h>

// Include necessary headers
#include "../boot/plinkman.h"
#include "../boot/psetting.h"
#include "../boot/pmain.h"
#include "../boot/console.h"
#include "../stuff/poutput.h"
#include "../stuff/imsrtns.h"
#include "../dev/mchasm.h"
#include "../dev/deviwave.h"
#include "../stuff/poutput-sdl2.h"
#include "../dev/player.h"
#include "../dev/mcp.h"
#include "../dev/deviplay.h"
#include "../cpiface/cpiface.h"
#include "../stuff/poutput-fontengine.h"
#include "../stuff/cp437.h"
#include "../filesel/mdb.h"
#include "../filesel/dirdb.h"
#include "../filesel/pfilesel.h"
#include "../filesel/filesystem.h"
#include "../filesel/filesystem-setup.h"
#include "../stuff/poll.h"

void wasm_plr_note_output_rate(uint32_t rate);

/* Ask the file selector to reread the directory it is showing.
 * Visitor files are written into /music on the Emscripten FS, which
 * filesystem-unix.c lists with opendir. */
EMSCRIPTEN_KEEPALIVE
void wasm_filesel_rescan(void)
{
	fsRescanCurrentDir();
}



// Plugin system - now using real dynamic linking via boot/plinkman.c
// No stubs needed - boot/plinkman.c handles plugin loading with Emscripten's dlopen

// Directory database API is now provided by filesel/dirdb.c

// Console stubs - these are defined as macros in poutput.h, so we don't redefine them

int conInit(void) {
    return 1;
}

// File system stubs for MDB
// MDB functions are now in ../filesel/mdb.c - removed stubs

// Stub for ancient (compressed file) support - not needed in WASM
struct ocpfilehandle_t *ancient_filehandle (char *compressionmethod, int compressionmethod_len, struct ocpfilehandle_t *s) {
    return NULL; // No compressed file support in WASM for now
}

// fsWriteModInfo is now provided by filesel/pfilesel.c

// OS file system support
#include "../stuff/file.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct osfile_t {
    int fd;
};

static struct osfile_t *osfile_alloc(int fd) {
    struct osfile_t *f = calloc(1, sizeof(*f));
    if (!f) {
        close(fd);
        return NULL;
    }
    f->fd = fd;
    return f;
}

struct osfile_t *osfile_open_readwrite(const char *pathname, int dolock, int mustcreate) {
    int flags = O_RDWR | O_CREAT;
    int fd;

    if (mustcreate) {
        flags |= O_EXCL;
    }

    fd = open(pathname, flags, 0666);
    if (fd < 0) {
        if (!(mustcreate && errno == EEXIST)) {
            fprintf(stderr, "open(%s): %s\n", pathname, strerror(errno));
        }
        return NULL;
    }

    (void)dolock; /* Advisory locks are not supported under wasm - ignore */
    return osfile_alloc(fd);
}

struct osfile_t *osfile_open_readonly(const char *pathname, int dolock) {
    int fd = open(pathname, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open(%s): %s\n", pathname, strerror(errno));
        return NULL;
    }
    (void)dolock;
    return osfile_alloc(fd);
}

void osfile_close(struct osfile_t *f) {
    if (!f) {
        return;
    }
    if (f->fd >= 0) {
        close(f->fd);
    }
    free(f);
}

static ssize_t clamp_size(uint64_t size) {
    size_t max = (size_t)SSIZE_MAX;
    if (size > max) {
        return (ssize_t)max;
    }
    return (ssize_t)size;
}

int64_t osfile_read(struct osfile_t *f, void *data, uint64_t size) {
    int64_t total = 0;
    uint8_t *dst = data;

    if (!f) {
        errno = EBADF;
        return -1;
    }

    while (size) {
        ssize_t chunk = clamp_size(size);
        ssize_t res = read(f->fd, dst, (size_t)chunk);
        if (res < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (res == 0) {
            break; /* EOF */
        }
        dst += res;
        size -= (uint64_t)res;
        total += res;
    }
    return total;
}

int64_t osfile_write(struct osfile_t *f, const void *data, uint64_t size) {
    int64_t total = 0;
    const uint8_t *src = data;

    if (!f) {
        errno = EBADF;
        return -1;
    }

    while (size) {
        ssize_t chunk = clamp_size(size);
        ssize_t res = write(f->fd, src, (size_t)chunk);
        if (res < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (res == 0) {
            errno = EIO;
            return -1;
        }
        src += res;
        size -= (uint64_t)res;
        total += res;
    }
    return total;
}

uint64_t osfile_getpos(struct osfile_t *f) {
    off_t pos;
    if (!f) {
        errno = EBADF;
        return 0;
    }
    pos = lseek(f->fd, 0, SEEK_CUR);
    if (pos < 0) {
        return 0;
    }
    return (uint64_t)pos;
}

void osfile_setpos(struct osfile_t *f, uint64_t pos) {
    if (!f) {
        return;
    }
    while (lseek(f->fd, (off_t)pos, SEEK_SET) < 0) {
        if (errno != EINTR) {
            break;
        }
    }
}

uint64_t osfile_getfilesize(struct osfile_t *f) {
    struct stat st;
    if (!f) {
        errno = EBADF;
        return 0;
    }
    if (fstat(f->fd, &st) < 0) {
        return 0;
    }
    return (uint64_t)st.st_size;
}

void osfile_truncate_at(struct osfile_t *f, uint64_t pos) {
    if (!f) {
        return;
    }
    ftruncate(f->fd, (off_t)pos);
}

void osfile_purge_readahead_cache(struct osfile_t *f) {
    (void)f; /* No dedicated cache */
}

int64_t osfile_purge_writeback_cache(struct osfile_t *f) {
    if (!f) {
        errno = EBADF;
        return -1;
    }
#if defined(__EMSCRIPTEN__)
    /* fsync is available under emscripten, but MEMFS may return EINVAL - ignore */
    fsync(f->fd);
#else
    fsync(f->fd);
#endif
    return 0;
}

int osdir_size_start(struct osdir_size_t *state, const char *path) {
    (void)state;
    (void)path;
    errno = ENOSYS;
    return -1;
}

int osdir_size_iterate(struct osdir_size_t *state) {
    (void)state;
    return 0;
}

void osdir_size_cancel(struct osdir_size_t *state) {
    (void)state;
}

int osdir_trash_available(const char *path) {
    (void)path;
    return 0;
}

int osdir_trash_perform(const char *path) {
    (void)path;
    errno = ENOSYS;
    return -1;
}

int osdir_delete_start(struct osdir_delete_t *state, const char *path) {
    (void)state;
    (void)path;
    errno = ENOSYS;
    return -1;
}

int osdir_delete_iterate(struct osdir_delete_t *state) {
    (void)state;
    return 0;
}

void osdir_delete_cancel(struct osdir_delete_t *state) {
    (void)state;
}

// Error handling stubs
int errInit(void) {
    return 1;
}

// Note: Custom SDL rendering code removed - now using original SDL2 driver

// Note: Custom WASM display functions removed - now using original SDL2 implementations
// The original SDL2 driver provides all necessary display functionality

// Note: Custom WASM console driver removed - now using original sdl2ConsoleDriver
// The original SDL2 driver from stuff/poutput-sdl2.c already supports WASM builds
// and provides better compatibility with the rest of OCP's display system

// Console is already defined in boot/console.c - the SDL2 driver will be set by sdl2_init()

// Frame lock and keyboard functions are provided by the original OCP files
// framelock() - provided by stuff/framelock.c
// ekbhit(), egetch() - provided by stuff/poutput-keyboard.c

// plDisplaySetupTextMode and plSetTextMode are defined as macros in poutput.h

// All display-related functions and variables are defined as macros in poutput.h
// They reference the global Console structure

// Note: Custom display functions removed - handled by original SDL2 driver

// Graphics functions are all defined as macros in poutput.h

// File opening stubs for virtual filesystem
struct ocpfilehandle_t *wasm_file_open_readfile(const char *path);

// WASM file system integration
struct ocpfile_t *file_open_readfile(struct ocpdir_t *basedir, const char *file, int flags) {
    // Will be implemented in wasm-fileio.c
    return NULL;
}

// Compatibility function stubs for compat.c
#include <unistd.h>
#include <limits.h>

char *getwd(char *buf) {
    return getcwd(buf, PATH_MAX);
}

// Note: Custom DOS rendering functions removed - handled by original SDL2 driver with fontengine

/* WASM-specific wrapper to clamp Console.TextWidth after SDL2 driver sets it.
 * This prevents buffer overflows in text-mode UI components (like cpitrack)
 * when ultra-wide browser windows result in Console.TextWidth > CONSOLE_MAX_X.
 * See stuff/poutput-curses.c for the same clamping pattern on other platforms.
 */
static const struct consoleDriver_t *sdl2_driver_original = NULL;
static struct consoleDriver_t wasm_console_driver;

static void wasm_SetTextMode_wrapper(unsigned char mode)
{
	sdl2_driver_original->SetTextMode(mode);

	if (Console.TextWidth > CONSOLE_MAX_X)
	{
		Console.TextWidth = CONSOLE_MAX_X;
	}
}

int plDisplayInit(void)
{
	/*
	 * WASM-specific SDL2 configuration:
	 * - SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT: Ensures keyboard input is captured
	 *   when the canvas element has focus. Without this, keyboard events may not
	 *   reach the WASM module in the browser.
	 * - SDL_HINT_TOUCH_MOUSE_EVENTS: Enables touch-to-mouse event conversion for
	 *   mobile/tablet browsers, allowing the UI to respond to touch input.
	 */
	SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");

	if (sdl2_init())
	{
		printf("plDisplayInit: sdl2_init failed\n");
		return -1;
	}

	/* The desktop driver maps a left click to Enter and a right click to
	 * fullscreen. In a browser a click is how the canvas gets focus, so it
	 * must not open the highlighted entry. */
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);

#ifdef OCP_WASM_DEBUG_LOGGING
	printf("plDisplayInit: Using original SDL2 console driver\n");
#endif

	/* Wrap the SDL2 driver's SetTextMode to add CONSOLE_MAX_X clamping */
	sdl2_driver_original = Console.Driver;
	wasm_console_driver = *sdl2_driver_original;
	wasm_console_driver.SetTextMode = wasm_SetTextMode_wrapper;
	*(const struct consoleDriver_t **)&Console.Driver = &wasm_console_driver;

	return 0;
}

// configAPI is now provided by boot/psetting.c.
// Section names follow boot/pmain.c: CommandLine -c, then screensec/soundsec.
void wasm_init_config_sections(void) {
    const char *config = cfGetProfileString("CommandLine", "c", "defaultconfig");
    if (!config || !config[0]) {
        config = "defaultconfig";
    }
    configAPI.ConfigSec = config;
    configAPI.ScreenSec = cfGetProfileString(config, "screensec", "screen");
    configAPI.SoundSec = cfGetProfileString(config, "soundsec", "sound");
    if (!configAPI.ScreenSec || !configAPI.ScreenSec[0]) {
        configAPI.ScreenSec = "screen";
    }
    if (!configAPI.SoundSec || !configAPI.SoundSec[0]) {
        configAPI.SoundSec = "sound";
    }
}

void wasm_display_shutdown(void) {
    // Display shutdown is handled by sdl2_done() from the original SDL2 driver
    extern void sdl2_done(void);
    sdl2_done();
}
// Registration functions are now provided by filesel/pfilesel.c

// plrRegisterDriver() and mcpRegisterDriver() are provided by the original OCP files
// plrRegisterDriver() - provided by dev/deviplay.c  
// mcpRegisterDriver() - provided by dev/deviwave.c

// Additional missing functions for linking
// UTF-8 display and editing functions now provided by stuff/utf-8.c

int try_open_jpeg(const char *filename) {
    return 0; // No JPEG support in WASM
}

int try_open_png(const char *filename) {
    return 0; // No PNG support in WASM
}

// tmTimerHandler is now provided by stuff/poll.c (real implementation)

// Main function pointer for original OCP system is defined in boot/pmain.c

// plmpInit is provided by cpiface.c

// cpiface functions are provided by cpiface.c - no stubs needed
// The real plmpOpenFile, plmpCallBack, plmpCloseFile are in cpiface/cpiface.c
// The real plmpLateInit is also in cpiface/cpiface.c and will register the real plOpenCP interface

// Additional stubs for cpiface dependencies

// Stub for mdbUnregisterReadInfo
// mdbUnregisterReadInfo is now in ../filesel/mdb.c

// Stub function for PipeProcess
static int wasm_PipeProcess(const char *command, char **output) {
    return -1; // Not supported in WASM
}

// PipeProcess function pointer
int (*PipeProcess)(const char *command, char **output) = wasm_PipeProcess;

// Global state for WASM audio player
static void (*g_player_tick_func)(struct cpifaceSessionAPI_t *cpifaceSession) = NULL;
static struct cpifaceSessionAPI_t *g_cpifaceSession = NULL;

int wasm_get_current_audio_latency_ms(void);

// ---------------------------------------------------------------------------
// Latency-aware sample history for visual synchronisation (oscilloscope)
// ---------------------------------------------------------------------------

#define WASM_SCOPE_HISTORY_FRAMES 32768
#define WASM_SCOPE_MAX_LOGICAL    MAXLCHAN
#define WASM_SCOPE_MAX_PHYSICAL   MAXLCHAN

struct SampleHistory {
    int      channels;
    uint32_t rate;
    int16_t *buffer;
    size_t   capacity;   // number of frames held in buffer
    size_t   start;      // frame index of oldest sample
    size_t   count;      // number of frames currently stored
};

static struct SampleHistory wasm_scope_master_history[2];   // 0 = mono, 1 = stereo
static struct SampleHistory wasm_scope_logical_history[WASM_SCOPE_MAX_LOGICAL];
static struct SampleHistory wasm_scope_physical_history[WASM_SCOPE_MAX_PHYSICAL];

static void wasm_scope_history_clear(struct SampleHistory *history)
{
    if (!history) {
        return;
    }
    history->start = 0;
    history->count = 0;
    history->rate = 0;
}

static int wasm_scope_history_ensure(struct SampleHistory *history, int channels)
{
    if (!history) {
        return 0;
    }

    if (history->buffer && history->channels == channels) {
        return 1;
    }

    free(history->buffer);
    history->buffer = NULL;
    history->capacity = 0;
    history->channels = channels;

    if (channels <= 0) {
        return 0;
    }

    history->buffer = calloc((size_t)channels * WASM_SCOPE_HISTORY_FRAMES, sizeof(int16_t));
    if (!history->buffer) {
        history->channels = 0;
        return 0;
    }

    history->capacity = WASM_SCOPE_HISTORY_FRAMES;
    wasm_scope_history_clear(history);
    return 1;
}

static void wasm_scope_history_append(struct SampleHistory *history,
                                      const int16_t *samples,
                                      size_t frames,
                                      uint32_t rate)
{
    if (!history || !history->buffer || !frames) {
        return;
    }

    if (history->rate != rate) {
        wasm_scope_history_clear(history);
        history->rate = rate;
    }

    if (frames >= history->capacity) {
        // Keep only the newest portion that fits into the history buffer
        size_t offset_frames = frames - history->capacity;
        samples += offset_frames * history->channels;
        frames = history->capacity;
        history->start = 0;
        history->count = 0;
    }

    // Ensure there is room for the new frames
    if (frames > history->capacity - history->count) {
        size_t drop = frames - (history->capacity - history->count);
        if (drop >= history->count) {
            history->start = 0;
            history->count = 0;
        } else {
            history->start = (history->start + drop) % history->capacity;
            history->count -= drop;
        }
    }

    size_t write_index = (history->start + history->count) % history->capacity;
    size_t first_chunk = history->capacity - write_index;
    size_t chunk_frames = frames < first_chunk ? frames : first_chunk;
    size_t chunk_samples = chunk_frames * (size_t)history->channels;

    memcpy(&history->buffer[write_index * history->channels], samples, chunk_samples * sizeof(int16_t));

    if (frames > chunk_frames) {
        size_t remaining_frames = frames - chunk_frames;
        memcpy(&history->buffer[0], samples + chunk_samples, remaining_frames * (size_t)history->channels * sizeof(int16_t));
    }

    history->count += frames;
    if (history->count > history->capacity) {
        history->count = history->capacity;
        history->start = (write_index + frames) % history->capacity;
    }

    history->rate = rate;
}

static int wasm_scope_history_copy(const struct SampleHistory *history,
                                   size_t start_frame,
                                   size_t frames,
                                   int16_t *out)
{
    if (!history || !history->buffer || !out) {
        return 0;
    }
    if (start_frame + frames > history->count) {
        return 0;
    }

    size_t read_index = (history->start + start_frame) % history->capacity;
    size_t first_chunk = history->capacity - read_index;
    size_t chunk_frames = frames < first_chunk ? frames : first_chunk;
    size_t chunk_samples = chunk_frames * (size_t)history->channels;

    memcpy(out, &history->buffer[read_index * history->channels], chunk_samples * sizeof(int16_t));

    if (frames > chunk_frames) {
        size_t remaining_frames = frames - chunk_frames;
        memcpy(out + chunk_samples, &history->buffer[0], remaining_frames * (size_t)history->channels * sizeof(int16_t));
    }

    return 1;
}

static void wasm_scope_history_trim(struct SampleHistory *history, size_t frames_to_keep)
{
    if (!history || !history->buffer) {
        return;
    }
    if (frames_to_keep >= history->count) {
        return;
    }

    size_t drop = history->count - frames_to_keep;
    history->start = (history->start + drop) % history->capacity;
    history->count = frames_to_keep;
}

static size_t wasm_scope_latency_frames(uint32_t rate)
{
    if (!rate) {
        return 0;
    }
    int latency_ms = wasm_get_current_audio_latency_ms();
    if (latency_ms <= 0) {
        return 0;
    }
    uint64_t frames = ((uint64_t)latency_ms * (uint64_t)rate) / 1000ULL;
    if (frames > WASM_SCOPE_HISTORY_FRAMES - 1) {
        frames = WASM_SCOPE_HISTORY_FRAMES - 1;
    }
    return (size_t)frames;
}

static void wasm_scope_history_apply(struct SampleHistory *history,
                                     int16_t *buffer,
                                     unsigned int frames,
                                     uint32_t rate,
                                     int channels)
{
    if (!frames || !buffer) {
        return;
    }
    if (!wasm_scope_history_ensure(history, channels)) {
        return;
    }

    wasm_scope_history_append(history, buffer, frames, rate);

    size_t latency_frames = wasm_scope_latency_frames(rate);
    if (!latency_frames) {
        return;
    }

    size_t required_frames = latency_frames + frames;
    if (required_frames > history->count) {
        return;
    }

    size_t start = history->count - required_frames;
    if (!wasm_scope_history_copy(history, start, frames, buffer)) {
        return;
    }

    // Retain enough history for future calls (latency plus a safety margin)
    size_t keep = latency_frames + (frames * 4);
    if (keep > WASM_SCOPE_HISTORY_FRAMES) {
        keep = WASM_SCOPE_HISTORY_FRAMES;
    }
    wasm_scope_history_trim(history, keep);
}

static void wasm_scope_history_reset_all(void)
{
    for (size_t i = 0; i < 2; i++) {
        wasm_scope_history_clear(&wasm_scope_master_history[i]);
    }

    for (size_t i = 0; i < WASM_SCOPE_MAX_LOGICAL; i++) {
        wasm_scope_history_clear(&wasm_scope_logical_history[i]);
    }

    for (size_t i = 0; i < WASM_SCOPE_MAX_PHYSICAL; i++) {
        wasm_scope_history_clear(&wasm_scope_physical_history[i]);
    }
}

// Pointers to the original mixer callbacks so we can wrap them
static void (*wasm_scope_original_GetMasterSample)(int16_t *, unsigned int, uint32_t, int) = NULL;
static int  (*wasm_scope_original_GetLChanSample)(struct cpifaceSessionAPI_t *, unsigned int, int16_t *, unsigned int, uint32_t, int) = NULL;
static int  (*wasm_scope_original_GetPChanSample)(struct cpifaceSessionAPI_t *, unsigned int, int16_t *, unsigned int, uint32_t, int) = NULL;

static void wasm_scope_latency_GetMasterSample(int16_t *buf, unsigned int len, uint32_t rate, int opt)
{
    if (wasm_scope_original_GetMasterSample) {
        wasm_scope_original_GetMasterSample(buf, len, rate, opt);
        int channels = (opt & mcpGetSampleStereo) ? 2 : 1;
        wasm_scope_history_apply(&wasm_scope_master_history[channels == 2], buf, len, rate, channels);
    }
}

static int wasm_scope_latency_GetLChanSample(struct cpifaceSessionAPI_t *cpifaceSession,
                                             unsigned int ch,
                                             int16_t *buf,
                                             unsigned int len,
                                             uint32_t rate,
                                             int opt)
{
    if (!wasm_scope_original_GetLChanSample) {
        return 0;
    }

    int res = wasm_scope_original_GetLChanSample(cpifaceSession, ch, buf, len, rate, opt);

    if (res != 3 && ch < WASM_SCOPE_MAX_LOGICAL) {
        wasm_scope_history_apply(&wasm_scope_logical_history[ch], buf, len, rate, 1);
    }

    return res;
}

static int wasm_scope_latency_GetPChanSample(struct cpifaceSessionAPI_t *cpifaceSession,
                                             unsigned int ch,
                                             int16_t *buf,
                                             unsigned int len,
                                             uint32_t rate,
                                             int opt)
{
    if (!wasm_scope_original_GetPChanSample) {
        return 0;
    }

    int res = wasm_scope_original_GetPChanSample(cpifaceSession, ch, buf, len, rate, opt);

    if (res != 3 && ch < WASM_SCOPE_MAX_PHYSICAL) {
        wasm_scope_history_apply(&wasm_scope_physical_history[ch], buf, len, rate, 1);
    }

    return res;
}

void wasm_install_scope_latency_wrappers(struct cpifaceSessionAPI_t *session)
{
    if (!session) {
        return;
    }

    if (session->GetMasterSample && session->GetMasterSample != wasm_scope_latency_GetMasterSample) {
        wasm_scope_original_GetMasterSample = session->GetMasterSample;
        session->GetMasterSample = wasm_scope_latency_GetMasterSample;
        wasm_scope_history_reset_all();
    }

    if (session->GetLChanSample && session->GetLChanSample != wasm_scope_latency_GetLChanSample) {
        wasm_scope_original_GetLChanSample = session->GetLChanSample;
        session->GetLChanSample = wasm_scope_latency_GetLChanSample;
        wasm_scope_history_reset_all();
    }

    if (session->GetPChanSample && session->GetPChanSample != wasm_scope_latency_GetPChanSample) {
        wasm_scope_original_GetPChanSample = session->GetPChanSample;
        session->GetPChanSample = wasm_scope_latency_GetPChanSample;
        wasm_scope_history_reset_all();
    }
}

void wasm_scope_latency_reset(void)
{
    wasm_scope_history_reset_all();
}

// Forward declarations
void wasm_mcpSet(struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val);
int wasm_mcpGet(struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt);

// Note: plrGetRealMasterVolume is provided by dev/player.c

// Store original player's mcpGet for wrapping (forward declaration)
static int (*original_player_mcpGet)(struct cpifaceSessionAPI_t *, int, int);







// Device APIs - plrDevAPI is now provided by dev/deviplay.c
// const struct plrDevAPI_t *plrDevAPI = &wasm_plrDevAPI;  // Now provided by deviplay.c
// mcpDevAPI and mcpAPI are now provided by the real OCP system (deviwave.c)
// const struct mcpDevAPI_t *mcpDevAPI = &wasm_mcpDevAPI;  // Now provided by deviwave.c
// const struct mcpAPI_t *mcpAPI = &wasm_mcpAPI;  // Now provided by deviwave.c

static uint32_t wasm_last_known_output_rate = 44100;

void wasm_plr_note_output_rate(uint32_t rate)
{
    if (rate >= 8000 && rate <= 192000) {
        wasm_last_known_output_rate = rate;
    }
}

// cpifaceIdle is registered with pollTypeAudio.
void wasm_audio_tick(void) {
    if (g_player_tick_func && g_cpifaceSession) {
        g_player_tick_func(g_cpifaceSession);
    }
    tmTimerHandler(pollTypeAudio);
}

// Global playing state for WASM
extern int g_is_playing; // Defined in wasm-fileio.c

// WASM mcpSet implementation - handle audio control settings
void wasm_mcpSet(struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val) {
    // In WASM, we don't have real audio hardware to control
    // Just store basic settings for MOD playback
    if (ch == -1) {
        switch (opt) {
            case 0: // mcpMasterPause
                g_is_playing = !val;
                break;
            default:
                break;
        }
    }
}

// Get browser audio latency from JavaScript with caching
EM_JS(int, wasm_get_browser_audio_latency_ms, (), {
    // Cache the AudioContext reference and latency measurement
    if (!Module._cachedAudioContext) {
        Module._cachedAudioContext = null;
        Module._cachedBrowserLatency = -1;
        Module._lastLatencyCheck = 0;
    }

    try {
        const now = Date.now();
        const CACHE_DURATION = 100; // Re-check every 100ms for fresh measurements

        // Try to find and cache the AudioContext if we don't have it
        // Keep searching periodically in case SDL creates it later
        if (!Module._cachedAudioContext || (now - Module._lastLatencyCheck > CACHE_DURATION)) {
            // Check if our HTML hook captured the AudioContext
            if (Module._capturedAudioContext && !Module._cachedAudioContext) {
                Module._cachedAudioContext = Module._capturedAudioContext;
            }
        }

        // If we have a cached context, measure latency (with periodic refresh)
        if (Module._cachedAudioContext) {
            const ctx = Module._cachedAudioContext;

            // Only update if cache is stale
            if (now - Module._lastLatencyCheck > CACHE_DURATION) {
                Module._lastLatencyCheck = now;

                // outputLatency: hardware + OS buffering (Chrome/Edge, modern browsers)
                if (typeof ctx.outputLatency === 'number') {
                    Module._cachedBrowserLatency = Math.round(ctx.outputLatency * 1000);
                    return Module._cachedBrowserLatency;
                }

                // baseLatency: minimum possible latency (fallback)
                if (typeof ctx.baseLatency === 'number') {
                    Module._cachedBrowserLatency = Math.round(ctx.baseLatency * 1000);
                    return Module._cachedBrowserLatency;
                }

                // Context exists but no latency API - use educated guess
                Module._cachedBrowserLatency = 15;
                return Module._cachedBrowserLatency;
            }

            // Return cached value if still fresh
            if (Module._cachedBrowserLatency > 0) {
                return Module._cachedBrowserLatency;
            }
        }

        // No audio context available yet, return conservative fallback
        // This will update once SDL creates the context
        return 50;
    } catch (e) {
        return 50; // Safe fallback
    }
});

// Forward declaration for player device API
extern const struct plrDevAPI_t *plrDevAPI;

// Get total audio pipeline latency (browser + ringbuffer)
int wasm_get_current_audio_latency_ms(void) {
    int browser_latency = wasm_get_browser_audio_latency_ms();
    int buffer_latency = 0;

    // Get ringbuffer fill level if available
    if (plrDevAPI && plrDevAPI->GetStats) {
        uint64_t committed = 0;
        uint64_t processed = 0;

        plrDevAPI->GetStats(&committed, &processed);

        // Calculate buffer fill in samples
        uint64_t buffer_samples = (committed > processed) ? (committed - processed) : 0;

        // Get current sample rate from player device API (or cached fallback)
        uint32_t rate = 0;
        if (plrDevAPI->GetRate) {
            rate = plrDevAPI->GetRate();
            if (rate) {
                wasm_plr_note_output_rate(rate);
            }
        }
        if (!rate) {
            rate = wasm_last_known_output_rate;
        }

        // Convert samples to milliseconds
        if (rate > 0 && buffer_samples > 0) {
            buffer_latency = (int)((buffer_samples * 1000) / rate);
        }
    }

    // Total latency = browser output + buffered audio
    // Add small safety margin for processing
    int total_latency = browser_latency + buffer_latency + 20;

    return total_latency;
}

// WASM mcpGet implementation - get audio control settings with latency offset
int wasm_mcpGet(struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt) {
    #include "dev/mcp.h"
    #include <stdio.h>

    int real_value = 0;

    // Call original player's mcpGet if available
    if (original_player_mcpGet) {
        real_value = original_player_mcpGet(cpifaceSession, ch, opt);

        // Apply pattern view latency offset for timer queries
        // Timer is in units of 1/65536 seconds, so convert ms to these units
        if (ch == -1 && opt == mcpGTimer) {
            int latency_ms = wasm_get_current_audio_latency_ms();
            int offset_ticks = (latency_ms * 65536) / 1000;
#ifdef OCP_WASM_DEBUG_LOGGING
            fprintf(stderr, "[WASM] mcpGTimer: real=%d, latency=%dms, offset=%d ticks, result=%d\n",
                    real_value, latency_ms, offset_ticks, real_value - offset_ticks);
#endif
            real_value -= offset_ticks;
        }

        return real_value;
    }

#ifdef OCP_WASM_DEBUG_LOGGING
    fprintf(stderr, "[WASM] mcpGet fallback: ch=%d, opt=%d (no original_player_mcpGet)\n", ch, opt);
#endif

    // Return reasonable default values for different options if no player mcpGet
    if (ch == -1) {
        switch (opt) {
            case mcpGCmdTimer:
                return 100;
            case mcpMasterPause:
                return !g_is_playing;
            default:
                return 0;
        }
    }
    return 0;
}

// dmFile is now provided by filesel/filesystem-unix.c (real implementation)

// pollInit and pollClose are now provided by stuff/poll.c (real implementation)

// UTF8 functions (utf8_encoded_length, utf8_encode, utf8_decode) now provided by stuff/utf-8.c

// Configuration set variable stub - mcpset settings
struct settings {
    int16_t amp;    /* [4..508]   64=nominal */
    int16_t speed;  /* [16..2048] 256=nominal */
    int16_t pitch;  /* [16..2048] 256=nominal */
    int16_t pan;    /* [-64..64] 64=nominal */
    int16_t bal;    /* [-64..64] 0=nominal */
    int16_t vol;    /* [0..64] 64=nominal */
    int16_t srnd;   /* 0: normal stereo, 1: one channel is inverted */
    int16_t reverb; /* [-64..64] 0=nominal */
    int16_t chorus; /* [-64..64] 0=nominal */
    uint8_t filter;
    uint8_t useecho;
    uint8_t splock; /* 0: speed,pitch are independent, 1: speed,pitch are locked */
    uint8_t viewfx; /* 0: volume,panning,balance,surround, 1: echo,revert,chorus */
};

struct settings set = {
    .amp = 64,      // nominal
    .speed = 256,   // nominal
    .pitch = 256,   // nominal
    .pan = 64,      // nominal
    .bal = 0,       // nominal
    .vol = 64,      // nominal
    .srnd = 0,      // normal stereo
    .reverb = 0,    // nominal
    .chorus = 0,    // nominal
    .filter = 0,
    .useecho = 0,
    .splock = 1,    // locked
    .viewfx = 0     // volume, panning, balance, surround
};

// Link manager functions now provided by boot/plinkman.c (with dlopen support)

// TGA image reader stub - fix signature to match expected usage
int TGAread(const char *filename, void **data, int *width, int *height, int *bpp, int extra) {
    return -1; // TGA reading not supported in WASM
}

// filesystem_resolve_dirdb_file now provided by filesel/filesystem-drive.c

// Stub for cpiWurfel2Init to avoid directory access issues (since cpikube.c is excluded)
void cpiWurfel2Init(const struct configAPI_t *configAPI) {
    // Skip directory operations that cause crashes in WASM
}

// Stub for cpiWurfel2Done (also from cpikube.c)
void cpiWurfel2Done(void) {
    // Nothing to clean up in stub
}

// External symbol references for MOD player
extern struct linkinfostruct dllextinfo;
// plrRegisterDriver is now provided by dev/deviplay.c
// mcpRegisterDriver is now provided by dev/deviwave.c

// mdbRegisterReadInfo is now in ../filesel/mdb.c

// cpiface functions are provided by cpiface.c

// Add a debug wrapper around fsTypeRegister to see what interface names are being registered
void debug_fsTypeRegister(struct moduletype modtype, const char **description, const char *interfacename, const struct cpifaceplayerstruct *cp) {
    extern void fsTypeRegister(struct moduletype modtype, const char **description, const char *interfacename, const struct cpifaceplayerstruct *cp);

    char modtype_str[5];
    modtype_str[0] = modtype.string.c[0];
    modtype_str[1] = modtype.string.c[1];
    modtype_str[2] = modtype.string.c[2];
    modtype_str[3] = modtype.string.c[3];
    modtype_str[4] = '\0';


    fsTypeRegister(modtype, description, interfacename, cp);
}

// iconv stubs removed - MAIN_MODULE=1 provides real implementations from libc

// Keyboard functions are now provided by stuff/poutput-keyboard.c

// filesystem_drive_init now provided by filesel/filesystem-drive.c
// filesystem_m3u_register now provided by filesel/filesystem-playlist-m3u.c
// filesystem_pls_register now provided by filesel/filesystem-playlist-pls.c
// adbMetaInit now provided by filesel/adbmeta.c
// Other filesystem_*_register functions are stubs for unimplemented filesystems
void filesystem_bzip2_register() { }
void filesystem_gzip_register() { }
void filesystem_pak_register() { }
void filesystem_rpg_register() { }
void filesystem_tar_register() { }
void filesystem_Z_register() { }
#ifndef OCP_WASM_HAS_ZIP
void filesystem_zip_register() { }
#endif



// WASM ocpfile wrapper for files in /assets
struct wasm_ocpfile_t {
    struct ocpfile_t head;
    char *filepath;
    uint32_t dirdb_ref;
};





// filesystem_unix_init is now provided by filesel/filesystem-unix.c
// The real implementation will handle /assets, /music, and other directories
// through the standard Unix filesystem abstraction layer

/*
 * MusicBrainz is disabled for WASM builds for the following reasons:
 *
 * 1. DEPENDENCY COMPLEXITY: MusicBrainz requires libcurl and cJSON libraries,
 *    which would significantly increase the WASM bundle size and build complexity.
 *    The additional ~500KB+ of dependencies is not justified for a web player.
 *
 * 2. BROWSER SECURITY: MusicBrainz makes network requests to external APIs.
 *    In a browser environment, this would require:
 *    - CORS configuration on MusicBrainz servers (which we don't control)
 *    - Potential HTTPS/mixed content issues
 *    - User privacy concerns with automatic metadata fetching
 *
 * 3. FUNCTIONALITY: MusicBrainz provides metadata enrichment (album art, artist info)
 *    which is primarily useful for library management. In the web player context,
 *    users are typically playing individual files rather than managing large libraries,
 *    making this feature less critical.
 *
 * 4. ALTERNATIVE: If metadata is needed in the browser, it can be fetched via
 *    JavaScript using the Fetch API or similar, which has better CORS support
 *    and doesn't require compiling additional C libraries to WASM.
 *
 * This stub returns success (1) to allow the file selector initialization to
 * proceed normally. The player will work without metadata enrichment.
 */
int musicbrainz_init(const struct configAPI_t *configAPI) {
    (void)configAPI;
    fprintf(stderr, "WASM: MusicBrainz disabled (not needed for web player)\n");
    return 1; /* Return success to allow initialization to continue */
}

void musicbrainz_done(void) {
    /* No cleanup needed for disabled feature */
}

// fsFPSCurrent is now provided by framelock-wasm.c (real implementation)

// wasm_set_single_file stub - for backward compatibility with drag-drop interface
void wasm_set_single_file(struct ocpfilehandle_t *file) {
    // No-op in file selector mode - files are managed by modlist
    (void)file;
}

// File selector, modlist, and filesystem functions are now provided by:
// - filesel/pfilesel.c (fsFileSelect, fsPreInit, fsInit, fsLateInit, fsClose, fsLateClose)
// - filesel/modlist.c (modlist_create, modlist_get, modlist_getcur)
// - filesel/filesystem.c and filesystem-unix.c (filesystem operations)

// File selector interface functions now provided by filesel/pfilesel.c:
// - plRegisterInterface, plUnregisterInterface, plFindInterface
// - fsSetup, fsRegisterExt, fsTypeRegister, fsTypeUnregister
// - fsLoopMods, plScrType, fsWriteModInfo (screen mode is plScrType)

// Note: Custom DrawBar implementations removed - handled by original SDL2 driver via swtext_drawbar/swtext_idrawbar

// Help system is now provided by help/cphelper.c (no longer stubbed)
