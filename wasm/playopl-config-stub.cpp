#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "../types.h"

#include "stuff/err.h"

struct PipeProcessAPI_t;
struct configAPI_t;
struct PluginInitAPI_t;
struct PluginCloseAPI_t;

static char *dup_auto_string(void)
{
	char *result = static_cast<char *>(malloc(5));
	if (result)
	{
		memcpy(result, "auto", 5);
	}
	return result;
}

extern "C" {

OCP_INTERNAL char *opl_config_retrowave_device(const struct PipeProcessAPI_t *PipeProcess, const struct configAPI_t *configAPI)
{
	(void)PipeProcess;
	(void)configAPI;
	return dup_auto_string();
}

OCP_INTERNAL int opl_config_init(struct PluginInitAPI_t *API)
{
	(void)API;
	return errOk;
}

OCP_INTERNAL void opl_config_done(struct PluginCloseAPI_t *API)
{
	(void)API;
}

}
