# Modland WASM Stepper Helpers

The files in this directory host the non-blocking "stepper" state machines that let the modland.com dialogs run inside the WebAssembly build without rewiring the original plugin sources.

Each `.inc.c` file mirrors a matching synchronous dialog (setup, initialize, mirrors, cachedir, removecache). The parent `.c` file only pulls the helper in when `OCP_WASM_FILESEL_STEPPER` is defined, so native builds continue to use the original blocking implementation while the WASM wrapper (`wasm/modland-com-stepper.c`) swaps in these async variants at runtime.

Keeping the helpers adjacent to the rest of the modland plugin makes it easier to maintain shared logic and keeps the WASM-specific code out of the main control flow.
