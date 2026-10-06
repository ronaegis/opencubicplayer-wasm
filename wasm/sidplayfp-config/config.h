#ifndef _SIDPLAY_CONFIG_H
#define _SIDPLAY_CONFIG_H

#if defined(WASM_BUILD)
#include "wasm/config.h"
#else
#include "../config.h"
#endif
#include <stdint.h>

#define HAVE_CXX11 1
#define HAVE_CXX14 1
#define HAVE_CXX17 1

#define HAVE_SIDPLAYFP_BUILDERS_EXSID_H 0
#define HAVE_SIDPLAYFP_BUILDERS_HARDSID_H 0
#define HAVE_SIDPLAYFP_BUILDERS_RESIDFP_H 1
#define HAVE_SIDPLAYFP_BUILDERS_RESID_H 1

#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_UNISTD_H 1

/* WebAssembly builds are little endian */
#undef WORDS_BIGENDIAN

#define PACKAGE "libsidplayfp"
#define PACKAGE_NAME "libsidplayfp"
#define PACKAGE_TARNAME "libsidplayfp"
#define PACKAGE_VERSION "0.0.0-wasm"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_URL "https://github.com/OpenCubicPlayer"
#ifndef VERSION
#define VERSION PACKAGE_VERSION
#endif


#endif /* _SIDPLAY_CONFIG_H */
