/* OpenCP Module Player - WASM Compatibility Header
 *
 * Simplified compatibility functions for WASM build
 * Minimal set needed for MOD player functionality
 */

#ifndef _WASM_COMPAT_H
#define _WASM_COMPAT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// Basic type definitions for WASM
#ifndef OCP_INTERNAL
#define OCP_INTERNAL static
#endif

// File handle stub for WASM
struct ocpfilehandle_t {
    FILE* file;
    char* data;
    size_t size;
    size_t pos;
};

// Session API stub for WASM
struct cpifaceSessionAPI_t {
    void* dummy; // Placeholder
};

// Sample info structure
struct sampleinfo {
    uint32_t length;
    uint32_t loopstart;
    uint32_t loopend;
    uint32_t samprate;
    uint8_t type;
    char name[32];
};

// Basic endianness support (assume little endian for WASM)
#define int16_little(x)   (x)
#define int32_little(x)   (x)
#define uint16_little(x)  (x)
#define uint32_little(x)  (x)
#define uint64_little(x)  (x)
#define int16_big(x)      bswap_16(x)
#define int32_big(x)      bswap_32(x)
#define uint16_big(x)     bswap_16(x)
#define uint32_big(x)     bswap_32(x)

static inline uint16_t bswap_16(uint16_t x) {
    return ((x & 0xFF) << 8) | ((x & 0xFF00) >> 8);
}

static inline uint32_t bswap_32(uint32_t x) {
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) |
           ((x & 0xFF0000) >> 8) | ((x & 0xFF000000) >> 24);
}

// Time functions
static inline uint64_t clock_ms(void) {
    return (uint64_t)(clock() * 1000 / CLOCKS_PER_SEC);
}

// String utilities
// strupr is provided by emscripten, so we don't define it here

// Memory utilities for compatibility
static inline void *memrchr(const void *s, int c, size_t n) {
    char c2 = c;
    char *s2 = (char *)s + n - 1;
    while (n--) {
        if (*s2 == c2) return s2;
        s2--;
    }
    return NULL;
}

// Path utilities (simplified for WASM)
static inline void getext_malloc(const char *src, char **ext) {
    const char *dot = strrchr(src, '.');
    if (dot) {
        *ext = strdup(dot);
    } else {
        *ext = strdup("");
    }
}

// File I/O utilities for virtual filesystem
static inline struct ocpfilehandle_t* wasm_open_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    struct ocpfilehandle_t* handle = malloc(sizeof(struct ocpfilehandle_t));
    if (!handle) {
        fclose(f);
        return NULL;
    }

    handle->file = f;
    handle->data = NULL;
    handle->pos = 0;

    // Get file size
    fseek(f, 0, SEEK_END);
    handle->size = ftell(f);
    fseek(f, 0, SEEK_SET);

    return handle;
}

static inline void wasm_close_file(struct ocpfilehandle_t* handle) {
    if (handle) {
        if (handle->file) fclose(handle->file);
        if (handle->data) free(handle->data);
        free(handle);
    }
}

static inline size_t wasm_read_file(struct ocpfilehandle_t* handle, void* buffer, size_t size) {
    if (!handle || !handle->file) return 0;
    return fread(buffer, 1, size, handle->file);
}

static inline int wasm_seek_file(struct ocpfilehandle_t* handle, long offset, int whence) {
    if (!handle || !handle->file) return -1;
    return fseek(handle->file, offset, whence);
}

static inline long wasm_tell_file(struct ocpfilehandle_t* handle) {
    if (!handle || !handle->file) return -1;
    return ftell(handle->file);
}

// Logging/debug functions
#define printf_debug(...) printf(__VA_ARGS__)

#endif /* _WASM_COMPAT_H */