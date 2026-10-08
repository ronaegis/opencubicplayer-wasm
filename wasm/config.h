#ifndef _OCP_CONFIG_H
#define _OCP_CONFIG_H 1

#define _GNU_SOURCE 1
#define _FILE_OFFSET_BITS 64

/* Font encoding definition */
#define OCP_FONT "CP437"

/* Standard headers needed for WASM build */
#include <limits.h>
#include <sys/types.h>

/* WASM-specific configuration */
#define WASM_BUILD 1
/* #define SUPPORT_STATIC_PLUGINS 1 */  /* Disabled - manual init is safer for WASM */

// MAIN_MODULE=1 provides full libc, disable our stubs
#define HAVE_STRERROR 1
#define HAVE_USLEEP 1
/* Dynamic linking support enabled for side modules */
/* #define NO_DLOPEN 1 */  /* Disabled - we support dlopen via Emscripten */

/* Plugin system constants */
#define MAXDLLLIST 150
#define LIB_SUFFIX ".wasm"  /* WASM side modules use .wasm extension */

/* Avoid conflicts with emscripten functions */
#define HAVE_MEMRCHR 1
#define HAVE_STRUPR 1
#define HAVE_GETWD 1

/* Function declarations for WASM compatibility */
char *getwd(char *buf);

/* Endian conversion functions are defined in types.h */

/* Module type constants */
#define mtMOD 1

/* Version information */
#define OCP_MAJOR_VERSION 3
#define OCP_MINOR_VERSION 5
#define OCP_PATCH_VERSION 0
#define DLLVERSION 0x30000001  /* 3.0.0.1 */

/* Console display constants. Upstream dialogs require CONSOLE_MIN_Y >= 20. */
#define CONSOLE_MIN_X 80
#define CONSOLE_MIN_Y 20
#define CONSOLE_MAX_X 132
#define CONSOLE_MAX_Y 60

/* Audio system constants for smpman.c */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Musl provides strverscmp. The SDL audio shim is force-included before
 * this header, so string.h is read before _GNU_SOURCE and the declaration
 * is skipped. Keep the prototype here and tell compat.c not to emit a
 * second copy. */
#define HAVE_STRVERSCMP 1
int strverscmp(const char *s1, const char *s2);

/* SDL2 support - emscripten uses SDL.h directly.
 * host-makedb/config.h reuses this file for a tool that has no SDL. */
#ifndef OCP_HOST_TOOL
#include <SDL.h>
#endif

/* Debug print macro: opt-in for verbose logging to avoid spamming browser consoles */
#ifndef PRINT
#ifdef OCP_WASM_DEBUG_LOGGING
#define PRINT(...) printf(__VA_ARGS__)
#else
#define PRINT(...) do {} while (0)
#endif
#endif

/* Internal visibility specifiers - let types.h define this */

/* Avoid conflicts with math.h bessel functions */
#include <math.h>
#ifdef MCP_NO_MATH_H
#define y0 vol_y0_var
#define y1 vol_y1_var
#endif

/* Disable unsupported features for WASM */
#undef HAVE_CURSES
#undef HAVE_CURSES_ENHANCED
#undef HAVE_CURSES_COLOR
#undef HAVE_CURSES_OBSOLETE
#undef HAVE_NCURSESW
#undef HAVE_NCURSES
#undef HAVE_CURSES_H
#undef HAVE_NCURSESW_H
#undef HAVE_NCURSES_H
#undef HAVE_NCURSESW_CURSES_H
#undef HAVE_NCURSES_CURSES_H

#undef HAVE_X11
#undef HAVE_XPM
#undef HAVE_FRAMEBUFFER

/* Enable SDL2 for WASM */
#define HAVE_SDL2 1

/* Audio formats - enable only MOD support initially */
#define HAVE_XM_SUPPORT 1
#undef HAVE_MP3_SUPPORT
#undef HAVE_OGG_SUPPORT
#undef HAVE_FLAC_SUPPORT

/* Standard library features */
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_UNISTD_H 1

/* Function availability */
#define HAVE_STRDUP 1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1

/* Math functions */
#define HAVE_MATH_H 1
#define HAVE_SQRT 1
#define HAVE_SIN 1
#define HAVE_COS 1

/* File system */
#define HAVE_STAT 1
#define HAVE_LSTAT 1

/* Endianness (assume little-endian for WASM) */
#undef WORDS_BIGENDIAN

/* Memory alignment */
#define SIZEOF_VOID_P 4  /* WASM is 32-bit pointers */

/* Key definitions for interface */
#define KEY_TAB 9
#define KEY_CTRL_P 0x10

/* Directory separators */
#define DIR_SUFFIX "/"

/* Configuration directories */
#define OPENCUBICPLAYER_SYSCONFDIR "/assets"
#define OPENCUBICPLAYER_DOCDIR "/assets"
#define OPENCUBICPLAYER_DATADIR "/assets"

/* Font paths for WASM - use our embedded font */
#define UNIFONT_OTF "/unifont-16.0.04.ttf"
#define UNIFONT_TTF "/unifont-16.0.04.ttf"
#define UNIFONT_UPPER_OTF "/unifont-16.0.04.ttf"
#define UNIFONT_UPPER_TTF "/unifont-16.0.04.ttf"

/* Virtual keys for WASM */
#define VIRT_KEY_RESIZE 0x2000
#define KEY_ESC 27
#define KEY_DELETE 127
#define _KEY_ENTER 13

/* Additional key definitions needed by poutput-sdl2.c */
#define KEY_INSERT 0x300
#define KEY_SHIFT_TAB 0x301
#define KEY_CTRL_BS 0x302
#define KEY_CTRL_ENTER 0x303
#define KEY_CTRL_D 0x04
#define KEY_CTRL_H 0x304
#define KEY_CTRL_J 0x0A
#define KEY_CTRL_K 0x0B
#define KEY_CTRL_L 0x0C
#define KEY_CTRL_Q 0x11
#define KEY_CTRL_S 0x13
#define KEY_CTRL_Z 0x1A

/* Macro for shift function keys */
#define KEY_SHIFT_F(x) (0x330 + (x) - 1)

/* Control navigation keys */
#define KEY_CTRL_UP 0x340
#define KEY_CTRL_DOWN 0x341
#define KEY_CTRL_RIGHT 0x342
#define KEY_CTRL_LEFT 0x343
#define KEY_CTRL_PGUP 0x344
#define KEY_CTRL_PGDN 0x345
#define KEY_CTRL_HOME 0x346
#define KEY_CTRL_END 0x347
#define KEY_CTRL_DELETE 0x348
#define KEY_CTRL_INSERT 0x349

/* Macro for ctrl function keys */
#define KEY_CTRL_F(x) (0x360 + (x) - 1)

/* Macro for ctrl+shift function keys */
#define KEY_CTRL_SHIFT_F(x) (0x370 + (x) - 1)

/* Alt keys A-Z */
#define KEY_ALT_A 0x200
#define KEY_ALT_B 0x201
#define KEY_ALT_C 0x202
#define KEY_ALT_D 0x203
#define KEY_ALT_E 0x204
#define KEY_ALT_F 0x205
#define KEY_ALT_G 0x206
#define KEY_ALT_H 0x207
#define KEY_ALT_I 0x208
#define KEY_ALT_J 0x209
#define KEY_ALT_K 0x20A
#define KEY_ALT_L 0x20B
#define KEY_ALT_M 0x20C
#define KEY_ALT_N 0x20D
#define KEY_ALT_O 0x20E
#define KEY_ALT_P 0x20F
#define KEY_ALT_Q 0x210
#define KEY_ALT_R 0x211
#define KEY_ALT_S 0x212
#define KEY_ALT_T 0x213
#define KEY_ALT_U 0x214
#define KEY_ALT_V 0x215
#define KEY_ALT_W 0x216
#define KEY_ALT_X 0x217
#define KEY_ALT_Y 0x218
#define KEY_ALT_Z 0x219

/* Additional alt keys */
#define KEY_ALT_ENTER 0x21A

/* No dynamic library loading in WASM */
#undef HAVE_DLOPEN
#undef HAVE_DLFCN_H

/* Unicode support */
#define HAVE_ICONV 1

/* Compiler extensions */
#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
#endif

/* Include stub for curses replacement */
#include "no-curses.h"

/* Re-apply sidplay-specific package macros that may be cleared in sidplay.cpp */
#include "sidplayfp-config/reapply.h"

#endif /* _OCP_CONFIG_H */
