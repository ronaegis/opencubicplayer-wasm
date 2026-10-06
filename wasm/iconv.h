#ifndef _ICONV_H_WASM_STUB
#define _ICONV_H_WASM_STUB

/* iconv stub for WASM - character conversion not available */
#include <stddef.h>

typedef void* iconv_t;

iconv_t iconv_open(const char *tocode, const char *fromcode);
size_t iconv(iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
int iconv_close(iconv_t cd);

#endif