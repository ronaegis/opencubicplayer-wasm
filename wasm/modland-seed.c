#include "config.h"
#include <emscripten.h>
#include "modland-seed.h"

static int (*wasm_modland_seed_fn)(const struct DevInterfaceAPI_t *API);

EMSCRIPTEN_KEEPALIVE
void wasm_modland_register_seed(int (*tick)(const struct DevInterfaceAPI_t *API))
{
	wasm_modland_seed_fn = tick;
}

int wasm_modland_seed_is_active(void)
{
	return wasm_modland_seed_fn != 0;
}

int wasm_modland_seed_tick(const struct DevInterfaceAPI_t *API)
{
	if (!wasm_modland_seed_fn)
	{
		return 0;
	}
	return wasm_modland_seed_fn(API);
}
