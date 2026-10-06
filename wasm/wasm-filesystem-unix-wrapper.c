/* OpenCP Module Player - WASM filesystem-unix wrapper
 * Adds just-in-time sample downloads for the WASM build.
 */

#include "wasm-fileio.h"
#include <emscripten/emscripten.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

/* Prevent the original init/done functions from being compiled */
#define filesystem_unix_init original_filesystem_unix_init
#define filesystem_unix_done original_filesystem_unix_done
#define unix_dir_steal original_unix_dir_steal
#define unix_file_steal original_unix_file_steal
#define unix_dir_readdir_iterate original_unix_dir_readdir_iterate

/* Include the original filesystem-unix implementation */
#include "../filesel/filesystem-unix.c"

/* Undefine so we can provide our own */
#undef filesystem_unix_init
#undef filesystem_unix_done
#undef unix_dir_steal
#undef unix_file_steal
#undef unix_dir_readdir_iterate

/* Forward declare our wrapper */
static struct ocpfilehandle_t *wasm_unix_file_open_wrapper(struct ocpfile_t *_s);

/* Override unix_file_steal so every file object routes through the downloader */
static struct ocpfile_t *unix_file_steal(struct ocpdir_t *parent, const uint32_t dirdb_node, uint64_t filesize)
{
	/* Call the original function */
	struct ocpfile_t *file = original_unix_file_steal(parent, dirdb_node, filesize);

	if (file)
	{
		/* Replace the real_open function with our wrapper (open is cache wrapper) */
#ifndef FILEHANDLE_CACHE_DISABLE
		file->real_open = wasm_unix_file_open_wrapper;
#else
		file->open = wasm_unix_file_open_wrapper;
#endif
	}

	return file;
}

/* Our wrapper for unix_file_open that downloads files on-demand */
static struct ocpfilehandle_t *wasm_unix_file_open_wrapper(struct ocpfile_t *_s)
{
	struct unix_ocpfile_t *s = (struct unix_ocpfile_t *)_s;
	char *path = NULL;

	dirdbGetFullname_malloc(s->head.dirdb_ref, &path, DIRDB_FULLNAME_NODRIVE);

	if (path)
	{
		char *download_path = path;
		char *allocated_path = NULL;
		if (download_path[0] != '/')
		{
			size_t len = strlen(download_path);
			allocated_path = malloc(len + 2);
			if (allocated_path)
			{
				allocated_path[0] = '/';
				memcpy(allocated_path + 1, download_path, len + 1);
				download_path = allocated_path;
			}
		}

		/* Trigger download if needed - this is synchronous */
		int status = wasm_download_sample_if_needed(download_path);

		/* Check if download failed */
		if (status < 0)
		{
			if (allocated_path) free(allocated_path);
			free(path);
			return NULL;
		}

		/* Update cached filesize so callers can detect when real data is present */
		struct stat st;
		if (!stat(download_path, &st))
		{
			s->filesize = st.st_size;
		}

		if (allocated_path) free(allocated_path);
		free(path);
	}

	/* Now call the original unix_file_open */
	return unix_file_open(_s);
}

/* Override unix_dir_readdir_file to use our wrapper */
static struct ocpfile_t *(*original_unix_dir_readdir_file)(struct ocpdir_t *, uint32_t) = NULL;

static struct ocpfile_t *wasm_unix_dir_readdir_file_wrapper(struct ocpdir_t *_s, uint32_t dirdb_ref)
{
	/* Call original to create the file object */
	struct ocpfile_t *file = unix_dir_readdir_file(_s, dirdb_ref);

	if (file)
	{
		/* Replace the real_open function with our wrapper (open is cache wrapper) */
#ifndef FILEHANDLE_CACHE_DISABLE
		file->real_open = wasm_unix_file_open_wrapper;
#else
		file->open = wasm_unix_file_open_wrapper;
#endif
	}

	return file;
}

/* Forward declare patch_dir_vtable for use by readdir_dir wrapper */
static void patch_dir_vtable(struct ocpdir_t *dir);

/* Override unix_dir_readdir_dir to patch newly created directories */
static struct ocpdir_t *(*original_unix_dir_readdir_dir)(struct ocpdir_t *, uint32_t) = NULL;

static struct ocpdir_t *wasm_unix_dir_readdir_dir_wrapper(struct ocpdir_t *_s, uint32_t dirdb_ref)
{
	/* Call original to create the directory object */
	struct ocpdir_t *dir = original_unix_dir_readdir_dir(_s, dirdb_ref);

	if (dir)
	{
		/* Patch the new directory's vtable */
		patch_dir_vtable(dir);
	}

	return dir;
}

/* Save original readdir_iterate function pointer */
static int (*saved_original_readdir_iterate)(ocpdirhandle_pt) = NULL;

/* Thread-local storage for callback wrapping during iteration */
static void (*saved_callback_file)(void *, struct ocpfile_t *) = NULL;
static void *saved_callback_token = NULL;

/* Wrapper callback that patches files before delivering to the original callback */
static void wasm_patching_file_callback(void *token, struct ocpfile_t *file)
{
	if (file)
	{
		/* Patch the real_open function before delivering to original callback (open is cache wrapper) */
#ifndef FILEHANDLE_CACHE_DISABLE
		file->real_open = wasm_unix_file_open_wrapper;
#else
		file->open = wasm_unix_file_open_wrapper;
#endif
	}

	/* Call the original callback with the patched file */
	if (saved_callback_file)
	{
		saved_callback_file(saved_callback_token, file);
	}
}

/* Wrapper for unix_dir_readdir_iterate that intercepts file callbacks */
static int unix_dir_readdir_iterate(ocpdirhandle_pt h)
{
	/* Access the handle structure to wrap the callback */
	struct unix_ocpdirhandle_t *handle = (struct unix_ocpdirhandle_t *)h;

	/* Save the original callback */
	void (*orig_callback_file)(void *, struct ocpfile_t *) = handle->callback_file;
	void *orig_token = handle->token;

	/* Only wrap if there's a file callback */
	if (orig_callback_file)
	{
		/* Save for our wrapper to use */
		saved_callback_file = orig_callback_file;
		saved_callback_token = orig_token;

		/* Replace with our wrapper */
		handle->callback_file = wasm_patching_file_callback;
	}

	/* Call the original iterate function */
	int result = original_unix_dir_readdir_iterate(h);

	/* Restore original callback */
	if (orig_callback_file)
	{
		handle->callback_file = orig_callback_file;
		handle->token = orig_token;
		saved_callback_file = NULL;
		saved_callback_token = NULL;
	}

	return result;
}

/* Patch the directory vtable to use our wrappers */
static void patch_dir_vtable(struct ocpdir_t *dir)
{
	if (!dir) return;

	/* Patch readdir_file to wrap file objects */
	if (dir->readdir_file)
	{
		if (!original_unix_dir_readdir_file)
		{
			original_unix_dir_readdir_file = dir->readdir_file;
		}
		dir->readdir_file = wasm_unix_dir_readdir_file_wrapper;
	}

	/* Patch readdir_dir to patch newly created directories */
	if (dir->readdir_dir)
	{
		if (!original_unix_dir_readdir_dir)
		{
			original_unix_dir_readdir_dir = dir->readdir_dir;
		}
		dir->readdir_dir = wasm_unix_dir_readdir_dir_wrapper;
	}

	/* Patch readdir_iterate to intercept file callbacks */
	if (dir->readdir_iterate)
	{
		if (!saved_original_readdir_iterate)
		{
			saved_original_readdir_iterate = dir->readdir_iterate;
		}
		dir->readdir_iterate = unix_dir_readdir_iterate;
	}
}

/* Wrap unix_dir_steal to patch every directory as it's created */
static struct ocpdir_t *unix_dir_steal(struct ocpdir_t *parent, const uint32_t dirdb_node)
{
	/* Call the original to create the directory */
	struct ocpdir_t *dir = original_unix_dir_steal(parent, dirdb_node);

	if (dir)
	{
		/* Patch this directory's vtable */
		patch_dir_vtable(dir);
	}

	return dir;
}

/* Provide filesystem_unix_init that patches the directories */
int filesystem_unix_init(void)
{
	struct ocpdir_t *root = file_unix_root();
	struct ocpdir_t *newcwd;
	char *currentpath;

	dmFile = RegisterDrive("file:", root, root);

	if (dmFile && dmFile->basedir) {
		dirdbRef(dmFile->basedir->dirdb_ref, dirdb_use_dir);

		/* Patch the root directory vtable */
		patch_dir_vtable(dmFile->basedir);
	}

	root->unref(root);
	root = 0;

	currentpath = getcwd_malloc();
	newcwd = filesystem_unix_resolve_dir(currentpath);
	free(currentpath);
	currentpath = 0;
	if (newcwd)
	{
		if (dmFile->cwd)
		{
			dmFile->cwd->unref(dmFile->cwd);
			dmFile->cwd = 0;
		}
		dmFile->cwd = newcwd;

		/* Also patch the cwd directory */
		patch_dir_vtable(newcwd);
	}

	if (!(configAPI.HomeDir       = filesystem_unix_resolve_dir(configAPI.HomePath      ))) { fprintf(stderr, "Unable to resolve cfHome=%s\n",      configAPI.HomePath);       return -1; }
	if (!(configAPI.ConfigHomeDir = filesystem_unix_resolve_dir(configAPI.ConfigHomePath))) { fprintf(stderr, "Unable to resolve cfConfigHome=%s\n", configAPI.ConfigHomePath); return -1; }
	if (!(configAPI.DataHomeDir   = filesystem_unix_resolve_dir(configAPI.DataHomePath  ))) { fprintf(stderr, "Unable to resolve cfDataHome=%s\n",   configAPI.DataHomePath);   return -1; }
	if (!(configAPI.DataDir       = filesystem_unix_resolve_dir(configAPI.DataPath      ))) { fprintf(stderr, "Unable to resolve cfData=%s\n",       configAPI.DataPath);       return -1; }
	if (!(configAPI.TempDir       = filesystem_unix_resolve_dir(configAPI.TempPath      ))) { fprintf(stderr, "Unable to resolve cfTemp=%s\n",       configAPI.TempPath);       return -1; }

	return 0;
}

void filesystem_unix_done(void)
{
	if (dmFile && dmFile->basedir) {
		dirdbUnref(dmFile->basedir->dirdb_ref, dirdb_use_dir);
	}

	if (configAPI.HomeDir)       { configAPI.HomeDir      ->unref(configAPI.HomeDir);       configAPI.HomeDir       = 0; }
	if (configAPI.ConfigHomeDir) { configAPI.ConfigHomeDir->unref(configAPI.ConfigHomeDir); configAPI.ConfigHomeDir = 0; }
	if (configAPI.DataHomeDir)   { configAPI.DataHomeDir  ->unref(configAPI.DataHomeDir);   configAPI.DataHomeDir   = 0; }
	if (configAPI.DataDir)       { configAPI.DataDir      ->unref(configAPI.DataDir);       configAPI.DataDir       = 0; }
	if (configAPI.TempDir)       { configAPI.TempDir      ->unref(configAPI.TempDir);       configAPI.TempDir       = 0; }
}
