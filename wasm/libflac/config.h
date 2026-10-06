#ifndef FLAC_CONFIG_H
#define FLAC_CONFIG_H

#define CPU_IS_BIG_ENDIAN 0
#define CPU_IS_LITTLE_ENDIAN 1
#define ENABLE_64_BIT_WORDS 1

#define FLAC__CPU_ARM64 0
#define FLAC__CPU_IA32 0
#define FLAC__CPU_X86_64 0
#define FLAC__HAS_A64NEONINTRIN 0
#define FLAC__HAS_NEONINTRIN 0
#define FLAC__HAS_X86INTRIN 0
#define FLAC__HAS_OGG 0
#define FLAC__NO_ASM 1
#define FLAC__SYS_DARWIN 0
#define FLAC__SYS_LINUX 0
#define FLAC__USE_AVX 0

#define HAVE_BSWAP16 1
#define HAVE_BSWAP32 1
#define HAVE_FSEEKO 1
#define HAVE_ICONV 0
#define HAVE_INTTYPES_H 1
#define HAVE_LANGINFO_CODESET 0
#define HAVE_LROUND 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_WCHAR_H 1
#define HAVE_MEMCPY 1
#define HAVE_MEMSET 1
#define HAVE_MEMMOVE 1
#define HAVE_POW 1
#define HAVE_FLOOR 1
#define HAVE_LOG 1
#define HAVE_LABS 1

#define PACKAGE_NAME "flac"
#define PACKAGE_TARNAME "flac"
#define PACKAGE_VERSION "1.4.3"
#define PACKAGE_STRING "flac 1.4.3"
#define PACKAGE_BUGREPORT "https://github.com/xiph/flac"
#define PACKAGE_URL "https://www.xiph.org/flac/"

#define SIZEOF_OFF_T 8
#define SIZEOF_VOIDP 4

#define STDC_HEADERS 1
#define WORDS_BIGENDIAN 0

#endif
