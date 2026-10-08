/* config.h for modland-makedb, the host tool that build.sh compiles to write
 * CPMDLAND.DAT. The desktop config.h only exists after ./configure, which a
 * browser-only checkout never runs, so the tool reuses the browser build's
 * settings without SDL and without the browser-only code paths.
 */
#ifndef OCP_HOST_MAKEDB_CONFIG_H
#define OCP_HOST_MAKEDB_CONFIG_H 1

#define OCP_HOST_TOOL 1
#include "../config.h"
#undef WASM_BUILD

/* The browser build targets musl. Say what the host libc has instead, so
 * stuff/compat.h does not redeclare functions the host already declares. */
#undef HAVE_MEMRCHR
#undef HAVE_STRUPR
#if defined(__APPLE__)
/* types.h reads the SDK version from these before it declares clock_gettime. */
# define HAVE_AVAILABILITY_H 1
# define HAVE_AVAILABILITYMACROS_H 1
#endif
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
# define HAVE_STRLCPY 1
# define HAVE_STRLCAT 1
#elif defined(__GLIBC__)
# define HAVE_MEMRCHR 1
# if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 38)
#  define HAVE_STRLCPY 1
#  define HAVE_STRLCAT 1
# endif
#endif

#endif
