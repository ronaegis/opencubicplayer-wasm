/*
 * WASM-specific fix for url_pipe_open signature mismatch
 *
 * The dummy implementation in url_pipe.c has an outdated signature.
 * This macro renames the old function and provides a wrapper with the correct signature.
 */

#ifndef TIMIDITY_URL_PIPE_FIX_H
#define TIMIDITY_URL_PIPE_FIX_H

/* Rename the old dummy implementation to avoid conflict */
#define url_pipe_open url_pipe_open_old_signature

#endif /* TIMIDITY_URL_PIPE_FIX_H */
