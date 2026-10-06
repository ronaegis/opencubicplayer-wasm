#ifndef WASM_SDL_AUDIO_SHIM_H
#define WASM_SDL_AUDIO_SHIM_H

#ifdef __EMSCRIPTEN__
#include <SDL.h>

#undef SDL_LockAudioDevice
#undef SDL_UnlockAudioDevice
#undef SDL_LockAudio
#undef SDL_UnlockAudio

#define SDL_LockAudioDevice(device)   ((void)(device))
#define SDL_UnlockAudioDevice(device) ((void)(device))
#define SDL_LockAudio()               ((void)0)
#define SDL_UnlockAudio()             ((void)0)
#endif /* __EMSCRIPTEN__ */

#endif /* WASM_SDL_AUDIO_SHIM_H */
