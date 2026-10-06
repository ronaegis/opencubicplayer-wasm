#include <stdio.h>

static void wasm_poutput_exit(int status)
{
    fprintf(stderr, "[WASM] SDL2 requested exit(%d); ignoring to keep runtime alive.\n", status);
}

#define exit(status) do { wasm_poutput_exit(status); return; } while (0)

#include "../stuff/poutput-sdl2.c"

#undef exit
