# Upstream files this fork patches

Native builds leave these branches out unless the macros are set. The WASM CMake build defines `WASM_BUILD`, `__EMSCRIPTEN__`, `OCP_WASM_DIALOG_STEPPER`, and `OCP_WASM_FILESEL_STEPPER`.

| File | Guard | Why |
| --- | --- | --- |
| `cpiface/cpiface.c` | `__EMSCRIPTEN__` | Esc returns to the file selector instead of quitting the page |
| `dev/deviplay.c`, `dev/deviwave.c`, `devp/devpalsa.c` | `OCP_WASM_DIALOG_STEPPER` | Device dialogs run as non-blocking steppers |
| `playopl/oplconfig.cpp`, `playsid/sidconfig.c`, `playtimidity/timidityconfig.c` | `OCP_WASM_DIALOG_STEPPER` | Same stepper swap for format setup dialogs |
| `filesel/pfilesel.c`, `filesel/pfilesel.h` | `OCP_WASM_FILESEL_STEPPER`, plus `fsRescanCurrentDir` | File selector yields to the browser loop; visitor files trigger a rescan |
| `filesel/modland.com/modland-com-*.c` and `filesel/modland.com/wasm-stepper/` | `OCP_WASM_FILESEL_STEPPER` | Modland screens do not block the main thread |
| `filesel/filesystem-zip.c` | `OCP_NO_BZIP2` | Zip stays; bzip2-inside-zip is a stub |
| `filesel/filesystem-ancient.cpp`, `playsid/libsidplayfp-api.cpp`, `playsid/sidplay.cpp`, `playsid/sidpplay.cpp`, `playym/config.h` | `WASM_BUILD` | Include `wasm/config.h` on the browser build |
| `ocp.hlp` | generated help database, force-added | The WASM link consumes the blob. `.gitignore` still lists `ocp.hlp` |

`fsRescanCurrentDir` is a small addition on `filesel/pfilesel.c` so a file written into `/music` after startup shows up in the selector that is already on screen.
