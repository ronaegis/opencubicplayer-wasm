/* OpenCP Module Player - Platform Config/Path API
 * Copyright (c) 2025-2026 Christophe Thibault - Cross-platform path configuration
 *
 * This header defines platform-specific hooks for config paths.
 * Each platform (Unix, Windows, WASM) provides its own implementation.
 */

#ifndef PSETTING_PLATFORM_H
#define PSETTING_PLATFORM_H

struct configAPI_t;  /* Forward declaration */

/* Platform initialization for paths - called during startup */
int psetting_platform_init_paths(struct configAPI_t *config);

/* Platform-specific directory creation/verification */
int psetting_platform_ensure_directory(const char *path);

/* Platform-specific temp path resolution */
char *psetting_platform_get_temp_path(void);

/* Platform-specific home path resolution */
char *psetting_platform_get_home_path(void);

/* Platform-specific config path resolution */
char *psetting_platform_get_config_path(void);

/* Platform-specific data path resolution */
char *psetting_platform_get_data_path(void);

/* Allow runtime configuration of virtual FS paths (WASM only) */
int psetting_platform_set_virtual_path(const char *key, const char *value);
const char *psetting_platform_get_virtual_path(const char *key);

/* Platform-specific argc/argv handling (for WASM JS interaction) */
int psetting_platform_get_args(int *argc, char ***argv);

#endif /* PSETTING_PLATFORM_H */
