#ifndef WASM_MODLAND_SEED_H
#define WASM_MODLAND_SEED_H

struct DevInterfaceAPI_t;

/* Side module registers the catalog installer. NULL clears it. */
void wasm_modland_register_seed(int (*tick)(const struct DevInterfaceAPI_t *API));

int wasm_modland_seed_is_active(void);
int wasm_modland_seed_tick(const struct DevInterfaceAPI_t *API);

#endif
