/* OpenCP Module Player - WASM Platform Config/Path Implementation
 * Copyright (c) 2025 - WASM port with configurable virtual FS paths
 *
 * This file provides WASM-specific implementations for config paths,
 * allowing the browser front-end to configure paths through supported APIs.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include "../boot/psetting.h"
#include "psetting-platform.h"

/* Default WASM virtual FS paths (can be overridden) */
static struct {
	char *program_base;
	char *autoload_base;
	char *home_base;
	char *config_base;
	char *datahome_base;
	char *data_base;
	char *temp_base;
} wasm_paths = {
	.program_base = NULL,
	.autoload_base = NULL,
	.home_base = NULL,
	.config_base = NULL,
	.datahome_base = NULL,
	.data_base = NULL,
	.temp_base = NULL
};

/* Helper to duplicate a path string with trailing slash */
static char *path_dup_with_slash(const char *path)
{
	size_t len;
	char *result;

	if (!path) return NULL;

	len = strlen(path);
	result = malloc(len + 2);  /* +2 for possible '/' and '\0' */
	if (!result) return NULL;

	strcpy(result, path);
	if (len > 0 && path[len - 1] != '/') {
		result[len] = '/';
		result[len + 1] = '\0';
	}

	return result;
}

/* Initialize default WASM paths */
static void init_default_paths(void)
{
	if (!wasm_paths.program_base)
		wasm_paths.program_base = strdup("/program/");
	if (!wasm_paths.autoload_base)
		wasm_paths.autoload_base = strdup("/program/autoload/");
	if (!wasm_paths.home_base)
		wasm_paths.home_base = strdup("/home/web_user/");
	if (!wasm_paths.config_base)
		wasm_paths.config_base = strdup("/home/web_user/.ocp/");
	if (!wasm_paths.datahome_base)
		wasm_paths.datahome_base = strdup("/home/web_user/.ocp/data/");
	if (!wasm_paths.data_base)
		wasm_paths.data_base = strdup("/assets/");
	if (!wasm_paths.temp_base)
		wasm_paths.temp_base = strdup("/tmp/");
}

/* Platform initialization */
int psetting_platform_init_paths(struct configAPI_t *config)
{
	if (!config) return -1;

	/* Initialize defaults if not already set */
	init_default_paths();

	/* Set up configAPI paths */
	free(config->HomePath);
	config->HomePath = strdup(wasm_paths.home_base);

	free(config->ConfigHomePath);
	config->ConfigHomePath = strdup(wasm_paths.config_base);

	free(config->DataHomePath);
	config->DataHomePath = strdup(wasm_paths.datahome_base);

	free(config->DataPath);
	config->DataPath = strdup(wasm_paths.data_base);

	free(config->TempPath);
	config->TempPath = strdup(wasm_paths.temp_base);

	/* Set up global program paths */
	free(cfProgramPath);
	cfProgramPath = strdup(wasm_paths.program_base);

	free(cfProgramPathAutoload);
	cfProgramPathAutoload = strdup(wasm_paths.autoload_base);

	return 0;
}

/* Platform-specific directory creation */
int psetting_platform_ensure_directory(const char *path)
{
	if (!path) return -1;

	if (mkdir(path, 0777) && errno != EEXIST) {
		fprintf(stderr, "WASM: mkdir(%s) failed: %s\n", path, strerror(errno));
		return -1;
	}

	return 0;
}

/* Platform-specific path getters */
char *psetting_platform_get_temp_path(void)
{
	init_default_paths();
	return strdup(wasm_paths.temp_base);
}

char *psetting_platform_get_home_path(void)
{
	init_default_paths();
	return strdup(wasm_paths.home_base);
}

char *psetting_platform_get_config_path(void)
{
	init_default_paths();
	return strdup(wasm_paths.config_base);
}

char *psetting_platform_get_data_path(void)
{
	init_default_paths();
	return strdup(wasm_paths.data_base);
}

/* Runtime configuration API - allows JavaScript to set paths */
int psetting_platform_set_virtual_path(const char *key, const char *value)
{
	char *new_path;

	if (!key || !value) return -1;

	new_path = path_dup_with_slash(value);
	if (!new_path) return -1;

	if (strcmp(key, "program") == 0) {
		free(wasm_paths.program_base);
		wasm_paths.program_base = new_path;
		free(cfProgramPath);
		cfProgramPath = strdup(new_path);
	} else if (strcmp(key, "autoload") == 0) {
		free(wasm_paths.autoload_base);
		wasm_paths.autoload_base = new_path;
		free(cfProgramPathAutoload);
		cfProgramPathAutoload = strdup(new_path);
	} else if (strcmp(key, "home") == 0) {
		free(wasm_paths.home_base);
		wasm_paths.home_base = new_path;
		if (configAPI.HomePath) {
			free(configAPI.HomePath);
			configAPI.HomePath = strdup(new_path);
		}
	} else if (strcmp(key, "config") == 0) {
		free(wasm_paths.config_base);
		wasm_paths.config_base = new_path;
		if (configAPI.ConfigHomePath) {
			free(configAPI.ConfigHomePath);
			configAPI.ConfigHomePath = strdup(new_path);
		}
	} else if (strcmp(key, "datahome") == 0) {
		free(wasm_paths.datahome_base);
		wasm_paths.datahome_base = new_path;
		if (configAPI.DataHomePath) {
			free(configAPI.DataHomePath);
			configAPI.DataHomePath = strdup(new_path);
		}
	} else if (strcmp(key, "data") == 0 || strcmp(key, "assets") == 0) {
		free(wasm_paths.data_base);
		wasm_paths.data_base = new_path;
		if (configAPI.DataPath) {
			free(configAPI.DataPath);
			configAPI.DataPath = strdup(new_path);
		}
	} else if (strcmp(key, "temp") == 0) {
		free(wasm_paths.temp_base);
		wasm_paths.temp_base = new_path;
		if (configAPI.TempPath) {
			free(configAPI.TempPath);
			configAPI.TempPath = strdup(new_path);
		}
	} else {
		free(new_path);
		return -1;  /* Unknown key */
	}

	return 0;
}

const char *psetting_platform_get_virtual_path(const char *key)
{
	if (!key) return NULL;

	init_default_paths();

	if (strcmp(key, "program") == 0)
		return wasm_paths.program_base;
	else if (strcmp(key, "autoload") == 0)
		return wasm_paths.autoload_base;
	else if (strcmp(key, "home") == 0)
		return wasm_paths.home_base;
	else if (strcmp(key, "config") == 0)
		return wasm_paths.config_base;
	else if (strcmp(key, "datahome") == 0)
		return wasm_paths.datahome_base;
	else if (strcmp(key, "data") == 0 || strcmp(key, "assets") == 0)
		return wasm_paths.data_base;
	else if (strcmp(key, "temp") == 0)
		return wasm_paths.temp_base;

	return NULL;
}

/* Platform-specific argc/argv (WASM uses stored values) */
int psetting_platform_get_args(int *argc, char ***argv)
{
	/* In WASM, these are stored in wasm-original-main.c */
	extern int stored_argc;
	extern char **stored_argv;

	if (argc) *argc = stored_argc;
	if (argv) *argv = stored_argv;

	return 0;
}
