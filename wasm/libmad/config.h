/* config.h for libmad - WASM build */

/* Basic feature detection for WASM/Emscripten */
#define HAVE_ASSERT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

/* Optimize for speed */
#define OPT_SPEED 1

/* Package info */
#define PACKAGE "libmad"
#define VERSION "0.15.1b"

/* FPM (Fixed Point Math) - use 64-bit for WASM */
#define FPM_64BIT 1

/* Size of pointers */
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
