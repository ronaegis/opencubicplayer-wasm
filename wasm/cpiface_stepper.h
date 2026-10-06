#ifndef WASM_CPIFACE_STEPPER_H
#define WASM_CPIFACE_STEPPER_H

#include "filesel/pfilesel.h"
#include "cpiface/cpiface.h"

/* Get current audio pipeline latency dynamically
 * Measures browser output latency + ringbuffer fill
 * Returns latency in milliseconds
 */
int wasm_get_current_audio_latency_ms(void);

struct cpiface_stepper_state {
    int screen_open;
    interfaceReturnEnum stop;
    int initialized;
};

void cpiface_stepper_init(struct cpiface_stepper_state *state);
void cpiface_stepper_reset(struct cpiface_stepper_state *state);
interfaceReturnEnum cpiface_stepper_tick(struct cpiface_stepper_state *state);
interfaceReturnEnum cpiface_stepper_run_blocking(struct cpiface_stepper_state *state);

#endif /* WASM_CPIFACE_STEPPER_H */
