/* OpenCP Module Player - WASM File I/O Implementation
 * Copyright (c) 2025-2026 Christophe Thibault - WASM port file operations
 *
 * This file implements file operations for the WASM version using Emscripten's
 * virtual file system
 */

#include "config.h"
#include "../types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <errno.h>

#include "../filesel/filesystem.h"
#include "../filesel/mdb.h"
#include "../filesel/pfilesel.h"
#include "../filesel/dirdb.h"
#include "../stuff/err.h"
#include "../playxm/xmplay.h"
#include "../cpiface/cpiface.h"
#include "../cpiface/cpiface-private.h"
#include "../dev/mcp.h"
#include "../dev/player.h"

// Bridge helpers from wasm-stubs.c
extern void wasm_set_single_file(struct ocpfilehandle_t *file);
extern void wasm_plr_note_output_rate(uint32_t rate);

EM_JS(int, wasm_download_sample_if_needed, (const char *path), {
    if (!Module.sampleRemoteMap) {
        console.warn('[Samples] sampleRemoteMap missing for ' + UTF8ToString(path || 0));
        return 0;
    }

    var rawPath = UTF8ToString(path || 0);
    if (!rawPath || rawPath.length === 0) {
        return 0;
    }

    var seen = new Set();
    var candidates = [];
    function addCandidate(candidate) {
        if (!candidate || typeof candidate !== 'string') {
            return;
        }
        if (!candidate.length || seen.has(candidate)) {
            return;
        }
        seen.add(candidate);
        candidates.push(candidate);
    }

    addCandidate(rawPath);
    if (rawPath.charCodeAt(0) !== 47 /* '/' */) {
        addCandidate('/' + rawPath);
    }
    if (rawPath.slice(0, 6) === 'music/') {
        addCandidate(rawPath.slice(6));
        addCandidate('/' + rawPath);
    }
    if (typeof PATH !== 'undefined' && PATH.normalize) {
        addCandidate(PATH.normalize(rawPath));
        addCandidate(PATH.normalize('/' + rawPath));
    }

    var entry = null;
    var resolvedKey = null;
    for (var i = 0; i < candidates.length; i++) {
        var key = candidates[i];
        if (Module.sampleRemoteMap[key]) {
            entry = Module.sampleRemoteMap[key];
            resolvedKey = key;
            break;
        }
        if (typeof PATH !== 'undefined' && PATH.normalize) {
            var normalizedKey = PATH.normalize(key);
            if (Module.sampleRemoteMap[normalizedKey]) {
                entry = Module.sampleRemoteMap[normalizedKey];
                resolvedKey = normalizedKey;
                break;
            }
        }
    }

    if (!entry) {
        return 0;
    }

    var fsPath = entry.fsPath || resolvedKey || rawPath;
    if (fsPath.charCodeAt(0) !== 47 /* '/' */) {
        fsPath = '/' + fsPath;
    }
    if (typeof PATH !== 'undefined' && PATH.normalize) {
        fsPath = PATH.normalize(fsPath);
    }

    try {
        var lookup = Module.FS.lookupPath(fsPath);
        var node = lookup ? lookup.node : null;
        if (node && node.contents && node.contents.length > 0) {
            entry.downloaded = true;
            return 0;
        }
        if (node && node.contents === null && Module.FS.forceLoadFile) {
            Module.FS.forceLoadFile(fsPath);
            var relookup = Module.FS.lookupPath(fsPath);
            node = relookup ? relookup.node : null;
            if (node && node.contents && node.contents.length > 0) {
                entry.downloaded = true;
                return 1;
            }
        }
    } catch (e) {
        // ignore lookup errors
    }

    if (!entry.url) {
        console.warn('[Samples] Missing URL for ' + fsPath);
        return 0;
    }

    var result = Module.ccall(
        'wasm_sync_download_to_file',
        'number',
        ['string', 'string'],
        [entry.url, fsPath]
    );

    if (result === 0) {
        entry.downloaded = true;
        try {
            var stat = Module.FS.stat(fsPath);
            if (stat && typeof stat.size === 'number') {
                entry.size = stat.size;
                var finalNode = Module.FS.lookupPath(fsPath).node;
                if (finalNode) {
                    finalNode.usedBytes = stat.size;
                }
            }
        } catch (e) {
            console.warn('[Samples] Unable to stat downloaded file', fsPath, e);
        }
        return 1;
    }

    console.error('[Samples] Download failed for', fsPath, 'url', entry.url, 'rc=', result);
    return -1;
});

// Simple file handle wrapper for WASM
struct wasm_filehandle_t {
    struct ocpfilehandle_t head;
    FILE *fp;
    uint64_t filesize;
    char *filepath;
};

// WASM file operations
static int wasm_file_read(struct ocpfilehandle_t *_file, void *dst, int len) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    return fread(dst, 1, len, file->fp);
}

static int wasm_file_eof(struct ocpfilehandle_t *_file) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    return feof(file->fp);
}

static uint64_t wasm_file_getpos(struct ocpfilehandle_t *_file) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    return ftell(file->fp);
}

static int wasm_file_seek_set(struct ocpfilehandle_t *_file, int64_t pos) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    return fseek(file->fp, pos, SEEK_SET);
}

static uint64_t wasm_file_filesize(struct ocpfilehandle_t *_file) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    return file->filesize;
}

static int wasm_file_filesize_ready(struct ocpfilehandle_t *_file) {
    return 1; // Always ready
}

static const char *wasm_file_filename_override(struct ocpfilehandle_t *_file) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    return file->filepath;
}

static void wasm_file_ref(struct ocpfilehandle_t *_file) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    file->head.refcount++;
}

static int wasm_file_error(struct ocpfilehandle_t *_file) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    return file->fp ? ferror(file->fp) : 1;
}

static int wasm_file_ioctl(struct ocpfilehandle_t *_file, const char *cmd, void *ptr) {
    // Simple ioctl implementation - return not supported for all commands
    return -1;
}

static void wasm_file_unref(struct ocpfilehandle_t *_file) {
    struct wasm_filehandle_t *file = (struct wasm_filehandle_t *)_file;
    file->head.refcount--;
    if (file->head.refcount <= 0) {
        // Clean up dirdb reference if we have one
        if (file->head.dirdb_ref != 0 && file->head.dirdb_ref != DIRDB_NOPARENT) {
            extern const struct dirdbAPI_t dirdbAPI;
            dirdbAPI.Unref(file->head.dirdb_ref, dirdb_use_file);
        }

        if (file->fp) {
            fclose(file->fp);
        }
        free(file->filepath);
        free(file);
    }
}

// No need for separate vtable - function pointers are in the structure itself

// Open a file for reading from the WASM virtual filesystem
struct ocpfilehandle_t *wasm_file_open_readfile(const char *path) {
    const char *open_path = path;
    char *normalized = NULL;
    if (path && path[0] != '/') {
        size_t len = strlen(path);
        normalized = malloc(len + 2);
        if (normalized) {
            normalized[0] = '/';
            memcpy(normalized + 1, path, len + 1);
            open_path = normalized;
        }
    }

    int download_status = wasm_download_sample_if_needed(open_path);
    if (download_status < 0) {
        free(normalized);
        return NULL;
    }

    FILE *fp = fopen(open_path, "rb");
    if (!fp) {
        fprintf(stderr, "WASM: fopen failed for %s (errno=%d)\n", open_path ? open_path : "(null)", errno);
        free(normalized);
        return NULL;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Extract filename from path
    const char *filename = strrchr(open_path, '/');
    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = open_path;
    }

    // Create a dirdb entry for this file
    // In WASM, we'll use DIRDB_NOPARENT as the parent (root directory)
    extern const struct dirdbAPI_t dirdbAPI;
    uint32_t dirdb_ref = dirdbAPI.FindAndRef(DIRDB_NOPARENT, filename, dirdb_use_file);

    if (dirdb_ref == DIRDB_NOPARENT) {
        printf("Warning: failed to create dirdb entry for %s\n", filename);
        // Continue anyway - some parts of OCP might still work
        dirdb_ref = 0;
    } else {
    }

    // Allocate handle
    struct wasm_filehandle_t *handle = malloc(sizeof(struct wasm_filehandle_t));
    if (!handle) {
        if (dirdb_ref != 0 && dirdb_ref != DIRDB_NOPARENT) {
            dirdbAPI.Unref(dirdb_ref, dirdb_use_file);
        }
        fclose(fp);
        return NULL;
    }

    // Initialize handle using the helper function
    memset(handle, 0, sizeof(*handle));
    ocpfilehandle_t_fill(&handle->head,
        wasm_file_ref,
        wasm_file_unref,
        NULL, // origin - not needed for our simple case
        wasm_file_seek_set,
        wasm_file_getpos,
        wasm_file_eof,
        wasm_file_error,
        wasm_file_read,
        wasm_file_ioctl,
        wasm_file_filesize,
        wasm_file_filesize_ready,
        wasm_file_filename_override,
        dirdb_ref, // Now we have a valid dirdb reference
        1  // refcount
    );

    handle->fp = fp;
    handle->filesize = filesize;
    handle->filepath = strdup(open_path);

    free(normalized);

    return &handle->head;
}

// Forward declarations for OCP integration
extern const struct cpifaceplayerstruct xmpPlayer; // From playxm
extern void plFindInterface(struct moduletype modtype, const struct interfacestruct **i, const struct cpifaceplayerstruct **cp);

// Access to the global cpiface session
extern struct cpifaceSessionPrivate_t cpifaceSessionAPI;

// Global XM module - this is now defined in xmpplay.c
extern OCP_INTERNAL struct xmodule mod;

// Global state for current loaded file
static struct ocpfilehandle_t *g_current_file = NULL;
static const struct interfacestruct *g_current_interface = NULL;
static const struct cpifaceplayerstruct *g_current_cp = NULL;
static int g_is_loaded = 0;
int g_is_playing = 0;


// Playback control functions for JavaScript interface
EMSCRIPTEN_KEEPALIVE
void play() {
    printf("=== WASM play() function called ===\n");

    if (!g_is_loaded) {
        printf("play() aborted - no file loaded\n");
        return;
    }

    // Check if we have both MCP and PLR APIs
    extern const struct plrDevAPI_t *plrDevAPI;

    // Start playback on the audio device first
    printf("WASM: Starting audio playback...\n");
    printf("WASM: plrDevAPI = %p\n", (void*)plrDevAPI);
    if (plrDevAPI) {
        printf("WASM: plrDevAPI->Play = %p\n", (void*)plrDevAPI->Play);
    }

    if (plrDevAPI) {
        if (!cpifaceSessionAPI.Public.plrActive && plrDevAPI->Play) {
            uint32_t rate = 44100;
            enum plrRequestFormat format = PLR_STEREO_16BIT_SIGNED;
            printf("WASM: Calling plrDevAPI->Play with rate=%u, format=%d\n", rate, format);
            int result = plrDevAPI->Play(&rate, &format, g_current_file, &cpifaceSessionAPI.Public);
            printf("WASM: plrDevAPI->Play returned %d, final rate=%u\n", result, rate);
            if (result) {
                wasm_plr_note_output_rate(rate);
            }
            if (!result) {
                printf("plrDevAPI->Play failed\n");
            }
        } else {
            printf("WASM: skipping plrDevAPI->Play (plrActive=%d, Play=%p)\n",
                    cpifaceSessionAPI.Public.plrActive,
                    plrDevAPI->Play);
        }
    } else {
        printf("plrDevAPI is unavailable (plrDevAPI=%p, Play=%p)\n",
            (void*)plrDevAPI, plrDevAPI ? (void*)plrDevAPI->Play : NULL);
    }

    // Check if MCP player was properly initialized by the MOD player's Init()
    if (cpifaceSessionAPI.Public.mcpDevAPI) {
        // The MOD player's Init() should have already called mcpDevAPI->OpenPlayer()
        // We don't need to call it again here - that would be a double initialization
    } else {
        printf("mcpDevAPI is NULL - MOD player Init() may have failed\n");
    }

    // Then unpause the MCP system
    if (cpifaceSessionAPI.Public.mcpDevAPI) {
        cpifaceSessionAPI.Public.mcpSet(&cpifaceSessionAPI.Public, -1, mcpMasterPause, 0);
        g_is_playing = 1;
    } else {
        printf("Unable to resume playback - mcpDevAPI is NULL\n");
    }
}

EMSCRIPTEN_KEEPALIVE
void pause_playback() {
    if (!g_is_loaded) {
        return;
    }

    // Use OCP pause system
    if (cpifaceSessionAPI.Public.mcpDevAPI) {
        cpifaceSessionAPI.Public.mcpSet(&cpifaceSessionAPI.Public, -1, mcpMasterPause, 1);
        g_is_playing = 0;
    }
}

EMSCRIPTEN_KEEPALIVE
void stop() {
    if (!g_is_loaded) {
        return;
    }

    if (g_is_playing && g_current_interface && g_current_interface->Close) {
        // Close the file through the OCP system
        g_current_interface->Close();
        g_is_playing = 0;
        g_is_loaded = 0;
    }
}

// Function to write data from JavaScript to virtual filesystem
EMSCRIPTEN_KEEPALIVE
void write_file_data(const char *path, const unsigned char *data, int length) {
    FILE *fp = fopen(path, "wb");
    if (fp) {
        fwrite(data, 1, length, fp);
        fclose(fp);
    } else {
        printf("Failed to write file: %s\n", path);
    }
}

// Create a file handle directly from memory buffer (for interface use)
struct ocpfilehandle_t *wasm_create_file_from_memory(const char *filename, const uint8_t *data, size_t size) {
    // First write data to virtual filesystem
    char virtual_path[512];
    snprintf(virtual_path, sizeof(virtual_path), "/tmp/%s", filename);

    FILE *fp = fopen(virtual_path, "wb");
    if (!fp) {
        printf("Failed to create virtual file %s\n", virtual_path);
        return NULL;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        printf("Failed to write all data (%zu/%zu bytes)\n", written, size);
        return NULL;
    }

    // Now open it as a regular file handle
    return wasm_file_open_readfile(virtual_path);
}
