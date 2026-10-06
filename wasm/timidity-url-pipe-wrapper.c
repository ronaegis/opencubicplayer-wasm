/*
 * WASM-specific replacement for url_pipe.c
 *
 * The original url_pipe.c has a dummy implementation with an outdated signature.
 * This file replaces it entirely with the correct signature expected by url.h.
 *
 * This avoids modifying upstream timidity-git source files.
 */

#include "config.h"
#include "../playtimidity/timidity-git/timidity/timidity.h"
#include "../playtimidity/timidity-git/libarc/url.h"

#ifndef URL_DIR

/* Forward declaration */
static int url_pipe_check(const char *url_string);

/* URL module for pipe (disabled for WASM) */
struct URL_module url_pipe_module = {
    URL_pipe_t,			/* type */
    url_pipe_check,		/* URL checker */
    NULL,			/* initializer */
    NULL			/* open */
};

/* Dummy implementation with correct signature for WASM */
URL url_pipe_open(struct timiditycontext_t *c, const char *command) {
    (void)c;      /* Unused */
    (void)command; /* Unused */
    return NULL;   /* Pipes not supported in WASM */
}

static int url_pipe_check(const char *url_string) {
    (void)url_string;  /* Unused */
    return 0;  /* Pipes not supported in WASM */
}

#endif /* URL_DIR */
