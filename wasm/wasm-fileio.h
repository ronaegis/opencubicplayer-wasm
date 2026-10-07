/* OpenCP Module Player - WASM File I/O Header
 * Copyright (c) 2025-2026 Christophe Thibault - WASM port file operations
 */

#ifndef WASM_FILEIO_H
#define WASM_FILEIO_H

#include "../types.h"
#include "../filesel/filesystem.h"

// Function to create a file handle from memory buffer
struct ocpfilehandle_t *wasm_create_file_from_memory(const char *filename, const uint8_t *data, size_t size);

// Function to open a file from the WASM virtual filesystem
struct ocpfilehandle_t *wasm_file_open_readfile(const char *path);
int wasm_download_sample_if_needed(const char *path);

// WASM exports for JavaScript
extern void write_file_data(const char *path, const unsigned char *data, int length);
extern void play(void);
extern void pause_playback(void);
extern void stop(void);

#endif // WASM_FILEIO_H
