#ifndef OCP_WASM_PLAYXM_STDIO_SHIM_H
#define OCP_WASM_PLAYXM_STDIO_SHIM_H

#include <emscripten/emscripten.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define OCP_WASM_PLUGIN_STDERR ((FILE *)-1)

static inline int ocp_wasm_plugin_write_buffer(FILE *stream, const char *buffer, size_t length)
{
    if (!buffer || length == 0) {
        return 0;
    }

    if (stream == OCP_WASM_PLUGIN_STDERR || stream == NULL) {
        emscripten_log(EM_LOG_WARN, "%.*s", (int)length, buffer);
        return (int)length;
    }

    size_t written = fwrite(buffer, 1, length, stream);
    return (int)written;
}

static inline int ocp_wasm_plugin_vfprintf(FILE *stream, const char *format, va_list args)
{
    va_list length_args;
    va_copy(length_args, args);
    int needed = vsnprintf(NULL, 0, format, length_args);
    va_end(length_args);

    if (needed < 0) {
        return needed;
    }

    char stack_buffer[256];
    char *buffer = stack_buffer;
    size_t buffer_size = sizeof(stack_buffer);

    if ((size_t)(needed + 1) > buffer_size) {
        buffer = (char *)malloc((size_t)needed + 1);
        if (!buffer) {
            return needed;
        }
        buffer_size = (size_t)needed + 1;
    }

    va_list output_args;
    va_copy(output_args, args);
    vsnprintf(buffer, buffer_size, format, output_args);
    va_end(output_args);

    int result = ocp_wasm_plugin_write_buffer(stream, buffer, (size_t)needed);

    if (buffer != stack_buffer) {
        free(buffer);
    }

    return result;
}

static inline int ocp_wasm_plugin_fprintf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = ocp_wasm_plugin_vfprintf(stream, format, args);
    va_end(args);
    return result;
}

#ifdef stderr
#undef stderr
#endif
#define stderr OCP_WASM_PLUGIN_STDERR

#ifdef fprintf
#undef fprintf
#endif
#define fprintf ocp_wasm_plugin_fprintf

#endif /* OCP_WASM_PLAYXM_STDIO_SHIM_H */
