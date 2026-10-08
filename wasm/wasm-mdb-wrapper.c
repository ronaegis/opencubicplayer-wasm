/* OpenCP Module Player - WASM mdb wrapper
 * Wraps mdbScan to skip 0-byte placeholder files.
 * This allows lazy-loaded files to be properly scanned when downloaded.
 */

#include "config.h"
#include <emscripten.h>

/* Rename original mdbScan so we can provide our wrapper */
#define mdbScan original_mdbScan

/* Include the original mdb.c implementation */
#include "../filesel/mdb.c"

/* Undefine so we can provide our wrapper */
#undef mdbScan

/* Forward declaration for wasm_set_sample_metadata */
int wasm_set_sample_metadata(
	const char *path,
	uint32_t size_low,
	uint32_t size_high,
	const char *modtype,
	int channels,
	int playtime,
	const char *title,
	const char *composer,
	const char *artist,
	const char *style,
	const char *comment,
	const char *album,
	uint32_t date);

/* EM_JS function to iterate through sample manifest and apply metadata from C
 * This is called after mdb is initialized but before file selector displays.
 * Returns number of entries processed.
 */
EM_JS(int, wasm_apply_sample_metadata_from_manifest, (), {
	if (!Module.sampleRemoteMap) {
		return 0;
	}

	var processed = {};
	var count = 0;

	Object.keys(Module.sampleRemoteMap).forEach(function(key) {
		var info = Module.sampleRemoteMap[key];
		if (!info || !info.hasMetadata || !info.fsPath) {
			return;
		}
		// Only process each unique fsPath once
		if (processed[info.fsPath]) {
			return;
		}
		processed[info.fsPath] = true;

		// Use size=0 for mdb lookup since placeholder files are 0 bytes
		// The file selector will look up mdb entries using the placeholder's size (0)
		// not the actual remote file size
		var sizeLow = 0;
		var sizeHigh = 0;

		// Allocate strings on WASM heap
		var pathPtr = stringToNewUTF8(info.fsPath);
		var modtypePtr = stringToNewUTF8(info.modtype || "");
		var titlePtr = stringToNewUTF8(info.title || "");
		var composerPtr = stringToNewUTF8(info.composer || "");
		var artistPtr = stringToNewUTF8(info.artist || "");
		var stylePtr = stringToNewUTF8(info.style || "");
		var commentPtr = stringToNewUTF8(info.comment || "");
		var albumPtr = stringToNewUTF8(info.album || "");

		var result = _wasm_set_sample_metadata(
			pathPtr,
			sizeLow,
			sizeHigh,
			modtypePtr,
			info.channels || 0,
			info.playtime || 0,
			titlePtr,
			composerPtr,
			artistPtr,
			stylePtr,
			commentPtr,
			albumPtr,
			info.date || 0
		);

		// Free allocated strings
		_free(pathPtr);
		_free(modtypePtr);
		_free(titlePtr);
		_free(composerPtr);
		_free(artistPtr);
		_free(stylePtr);
		_free(commentPtr);
		_free(albumPtr);

		if (result) {
			count++;
		}
	});

	if (count > 0) {
		console.log('[WASM-MDB] Applied metadata for ' + count + ' sample files');
	}

	return count;
});

/* WASM wrapper for mdbScan that skips 0-byte placeholder files */
void mdbScan(struct ocpfile_t *file, uint32_t mdb_ref, struct ocpfilehandle_t **retain)
{
	/* Skip scanning if file is 0 bytes (lazy-loaded placeholder) */
	if (file && file->filesize && file->filesize(file) == 0)
	{
		/* Leave placeholder entries unscanned until real data is present.
		 * When the file is opened via fsGetPrevFile/fsGetNextFile,
		 * mdbInfoIsAvailable will return FALSE (no info stored),
		 * triggering mdbReadInfo on the now-downloaded file.
		 */
		return;
	}

	/* Call the original mdbScan for non-placeholder files */
	original_mdbScan(file, mdb_ref, retain);
}

/* WASM function to set sample metadata from JavaScript
 * This allows pre-populating mdb entries for lazy-loaded sample files
 * so their metadata shows in the file selector before download.
 *
 * Parameters:
 *   path       - Full path to the file (e.g., "/music/song.mod")
 *   size_low   - File size in bytes (low 32 bits)
 *   size_high  - File size in bytes (high 32 bits, usually 0)
 *   modtype    - 4-character module type string (e.g., "MOD ", "S3M ", "XM  ", "IT  ")
 *   channels   - Number of channels (0 if unknown)
 *   playtime   - Duration in seconds (0 if unknown)
 *   title      - Song title (NULL or empty string if unknown)
 *   composer   - Composer name (NULL or empty string if unknown)
 *   artist     - Artist name (NULL or empty string if unknown)
 *   style      - Style/genre (NULL or empty string if unknown)
 *   comment    - Comment (NULL or empty string if unknown)
 *   album      - Album name (NULL or empty string if unknown)
 *   date       - Date as YYYYMMDD integer (0 if unknown)
 *
 * Returns: 1 on success, 0 on failure
 */
EMSCRIPTEN_KEEPALIVE
int wasm_set_sample_metadata(
	const char *path,
	uint32_t size_low,
	uint32_t size_high,
	const char *modtype,
	int channels,
	int playtime,
	const char *title,
	const char *composer,
	const char *artist,
	const char *style,
	const char *comment,
	const char *album,
	uint32_t date)
{
	uint32_t dirdb_ref;
	uint32_t mdb_ref;
	struct moduleinfostruct m;
	uint64_t size = ((uint64_t)size_high << 32) | (uint64_t)size_low;

	if (!path || !path[0])
	{
		fprintf(stderr, "[WASM-MDB] wasm_set_sample_metadata: empty path\n");
		return 0;
	}

	/* Resolve path to dirdb_ref */
	dirdb_ref = dirdbResolvePathWithBaseAndRef(DIRDB_NOPARENT, path, DIRDB_RESOLVE_NODRIVE, dirdb_use_file);
	if (dirdb_ref == DIRDB_CLEAR)
	{
		fprintf(stderr, "[WASM-MDB] wasm_set_sample_metadata: failed to resolve path '%s'\n", path);
		return 0;
	}

	/* Get or create mdb reference */
	mdb_ref = mdbGetModuleReference2(dirdb_ref, size);
	if (mdb_ref == 0xffffffff)
	{
		fprintf(stderr, "[WASM-MDB] wasm_set_sample_metadata: failed to get mdb ref for '%s'\n", path);
		dirdbUnref(dirdb_ref, dirdb_use_file);
		return 0;
	}

	/* Initialize moduleinfostruct */
	memset(&m, 0, sizeof(m));
	m.size = size;
	m.channels = (uint8_t)channels;
	m.playtime = (uint16_t)playtime;
	m.date = date;

	/* Do NOT set module type for lazy-loaded files!
	 * Setting modtype makes mdbInfoIsAvailable() return TRUE,
	 * which prevents the file from being scanned when opened.
	 * The scan is what triggers the lazy download.
	 * Leave modtype as mtUnRead (0) so the file gets scanned on open.
	 */
	(void)modtype; /* Unused - intentionally not setting modtype */
	m.modtype.integer.i = 0; /* mtUnRead */

	/* Copy string fields */
	if (title && title[0])
	{
		snprintf(m.title, sizeof(m.title), "%s", title);
	}
	if (composer && composer[0])
	{
		snprintf(m.composer, sizeof(m.composer), "%s", composer);
	}
	if (artist && artist[0])
	{
		snprintf(m.artist, sizeof(m.artist), "%s", artist);
	}
	if (style && style[0])
	{
		snprintf(m.style, sizeof(m.style), "%s", style);
	}
	if (comment && comment[0])
	{
		snprintf(m.comment, sizeof(m.comment), "%s", comment);
	}
	if (album && album[0])
	{
		snprintf(m.album, sizeof(m.album), "%s", album);
	}

	/* Write to mdb */
	if (!mdbWriteModuleInfo(mdb_ref, &m))
	{
		fprintf(stderr, "[WASM-MDB] wasm_set_sample_metadata: failed to write mdb info for '%s'\n", path);
		dirdbUnref(dirdb_ref, dirdb_use_file);
		return 0;
	}

	/* Release dirdb ref (mdb has its own tracking) */
	dirdbUnref(dirdb_ref, dirdb_use_file);

	return 1;
}
