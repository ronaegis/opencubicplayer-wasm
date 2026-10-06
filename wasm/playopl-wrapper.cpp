/* OpenCP Module Player - WASM wrapper for playopl
 * Copyright (c) 2025 - WASM-specific .bnk file fallback handling
 *
 * This wrapper intercepts file loading to provide fallback paths for .bnk files,
 * enabling on-demand loading from /music/ directory without modifying the original file.
 *
 * Approach: Wrap ocpdir_readdir_file() inline to add fallback logic.
 */

#include "config.h"
#include "types.h"
#include <string.h>
#include <strings.h>

extern "C" {
	#include "filesel/dirdb.h"
	#include "filesel/filesystem.h"
}

// Our wrapper that tries fallback paths for .bnk files
static inline struct ocpfile_t *ocpdir_readdir_file_with_fallback(struct ocpdir_t *s, const char *name, const struct dirdbAPI_t *dirdb)
{
	// Try the original path first (inline the original implementation)
	uint32_t dirdb_ref;
	struct ocpfile_t *retval = 0;

	if (!s) return 0;

	dirdb_ref = dirdb->FindAndRef (s->dirdb_ref, name, dirdb_use_file);
	if (dirdb_ref != DIRDB_CLEAR) {
		retval = s->readdir_file (s, dirdb_ref);
		dirdb->Unref (dirdb_ref, dirdb_use_file);
	}

	// If not found and it's a .bnk file, try fallback path
	if (!retval && name)
	{
		const char *basename = strrchr(name, '/');
		if (!basename) basename = strrchr(name, '\\');
		basename = basename ? basename + 1 : name;

		size_t name_len = strlen(basename);
		if (name_len > 4 && strcasecmp(basename + name_len - 4, ".bnk") == 0)
		{
			// Try fallback path in /music/ directory
			char fallback_path[256];
			snprintf(fallback_path, sizeof(fallback_path), "/music/%s", basename);

			dirdb_ref = dirdb->FindAndRef (s->dirdb_ref, fallback_path, dirdb_use_file);
			if (dirdb_ref != DIRDB_CLEAR) {
				retval = s->readdir_file (s, dirdb_ref);
				dirdb->Unref (dirdb_ref, dirdb_use_file);
			}
		}
	}

	return retval;
}

// Redefine the function name so oplplay.cpp uses our wrapper
#define ocpdir_readdir_file ocpdir_readdir_file_with_fallback

// Now include the original oplplay.cpp - all calls to ocpdir_readdir_file will use our wrapper
#include "../playopl/oplplay.cpp"
