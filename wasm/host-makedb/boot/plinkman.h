#ifndef MODLAND_MAKEDB_PLINKMAN_WRAP_H
#define MODLAND_MAKEDB_PLINKMAN_WRAP_H
#include_next "boot/plinkman.h"
/* The build tool only keeps the catalog parser. A static descriptor that
 * nothing calls lets the compiler drop the file-browser UI. */
#undef DLLEXTINFO_CORE_PREFIX
#define DLLEXTINFO_CORE_PREFIX static
#endif
