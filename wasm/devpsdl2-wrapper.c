/* devpsdl2-wrapper.c - WASM-specific SDL2 audio device wrapper
 *
 * PURPOSE:
 *   Wraps devpsdl-common.c to use configurable SDL audio buffer size.
 *   The original SDL2 audio device hardcodes 125ms buffers, which is too large
 *   for players with smaller internal buffers (like HVL/AHX at 100ms).
 *
 * WHY WRAPPER APPROACH:
 *   - Cannot modify original devp/devpsdl-common.c (project policy)
 *   - WASM has different latency characteristics than desktop platforms
 *   - Need configurable buffer size to prevent audio starvation
 *
 * APPROACH:
 *   - Include devpsdl-common.c with renamed symbols
 *   - Override devpSDLPlay to read "sdl_buffer_ms" config option
 *   - WASM startup code sets this config value for platform-specific tuning
 *
 * Last updated: 2025-10-23
 */

#include "config.h"
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_audio.h>
#include "../types.h"
#include "../boot/plinkman.h"
#include "../boot/psetting.h"
#include "../cpiface/cpiface.h"
#include "../dev/deviplay.h"
#include "../dev/player.h"
#include "../dev/ringbuffer.h"
#include "../stuff/err.h"
#include "../stuff/imsrtns.h"

/* Include the common SDL device code with renamed symbols */
#define devpSDLPlay devpsdl_original_Play
#define devpSDL devpsdl_original_API

#include "../devp/devpsdl-common.c"

#undef devpSDLPlay
#undef devpSDL

/* WASM-specific devpSDLPlay that reads SDL buffer size from config */
static int devpSDLPlay (uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	SDL_AudioSpec desired, obtained;
	int plrbufsize; /* given in ms */
	int sdl_buffer_ms; /* SDL callback buffer size in ms */
	int buflength;

	PRINT("%s(*%d,*%d)\n", __FUNCTION__, *rate, *format);

	devpSDLInPause = 0;
	devpSDLPauseSamples = 0;

	*format = PLR_STEREO_16BIT_SIGNED;

	if (!*rate)
	{
		*rate = 44100;
	}
	if (*rate < 22050)
	{
		*rate = 22050;
	}
	if (*rate > 96000)
	{
		*rate = 96000;
	}

	/* Read SDL buffer size from config (WASM sets this at startup) */
	sdl_buffer_ms = cpifaceSession->configAPI->GetProfileInt2 (
		cpifaceSession->configAPI->SoundSec, "sound", "sdl_buffer_ms", 125, 10);

	/* Clamp to reasonable range: 10ms to 500ms */
	if (sdl_buffer_ms < 10)
	{
		sdl_buffer_ms = 10;
	}
	if (sdl_buffer_ms > 500)
	{
		sdl_buffer_ms = 500;
	}

	SDL_memset (&desired, 0, sizeof (desired));
	desired.freq = *rate;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = (*rate * sdl_buffer_ms) / 1000;
	desired.callback = theRenderProc;
	desired.userdata = NULL;

	fprintf(stderr, "[SDL2-WASM] Configuring audio: %u Hz, buffer: %u samples (~%d ms)\n",
	        *rate, desired.samples, sdl_buffer_ms);

#if SDL_VERSION_ATLEAST(2,0,18)
	lastCallbackTime = SDL_GetTicks64 ();
#else
	lastCallbackTime = SDL_GetTicks ();
#endif
	lastLength = 0;

#if SDL_VERSION_ATLEAST(2,0,0)
	status=SDL_OpenAudioDevice (NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
#else
	status=SDL_OpenAudio (&desired, &obtained);
#endif
	if (status < 0)
	{
		fprintf (stderr, "[SDL] SDL_OpenAudio returned %d (%s)\n", (int)status, SDL_GetError());
		free (devpSDLBuffer); devpSDLBuffer = 0;
		plrDriverAPI->ringbufferAPI->free (devpSDLRingBuffer); devpSDLRingBuffer = 0;
		return 0;
	}
	devpSDLRate = *rate = obtained.freq;

	fprintf(stderr, "[SDL2-WASM] Audio opened: %u Hz, buffer: %u samples (~%.1f ms)\n",
	        obtained.freq, obtained.samples, (obtained.samples * 1000.0) / obtained.freq);

	plrbufsize = cpifaceSession->configAPI->GetProfileInt2 (cpifaceSession->configAPI->SoundSec, "sound", "plrbufsize", 200, 10);
	/* clamp the plrbufsize to be atleast 150ms and below 1000 ms */
	if (plrbufsize < 150)
	{
		plrbufsize = 150;
	}
	if (plrbufsize > 1000)
	{
		plrbufsize = 1000;
	}
	buflength = devpSDLRate * plrbufsize / 1000;

	if (buflength < obtained.samples * 2)
	{
		buflength = obtained.samples * 2;
	}
	if (!(devpSDLBuffer=calloc (buflength, 4)))
	{
#if SDL_VERSION_ATLEAST(2,0,0)
		SDL_CloseAudioDevice (status);
		status=-1;
#else
		SDL_CloseAudio ();
#endif
		return 0;
	}

	if (!(devpSDLRingBuffer = plrDriverAPI->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, buflength)))
	{
#if SDL_VERSION_ATLEAST(2,0,0)
		SDL_CloseAudioDevice (status);
		status=-1;
#else
		SDL_CloseAudio ();
#endif
		free (devpSDLBuffer); devpSDLBuffer = 0;
		return 0;
	}

	cpifaceSession->GetMasterSample = plrDriverAPI->GetMasterSample;
	cpifaceSession->GetRealMasterVolume = plrDriverAPI->GetRealMasterVolume;
	cpifaceSession->plrActive = 1;

#warning This needs to delay until we have received the first commit
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_PauseAudioDevice (status, 0);
#else
	SDL_PauseAudio (0);
#endif
	return 1;
}

/* Re-export devpSDL API with our wrapped Play function */
static const struct plrDevAPI_t devpSDL = {
	devpSDLIdle,
	devpSDLPeekBuffer,
	devpSDLPlay,
	devpSDLGetBuffer,
	devpSDLGetRate,
	devpSDLOnBufferCallback,
	devpSDLCommitBuffer,
	devpSDLPause,
	devpSDLStop,
	0,
	0,
	devpSDLGetStats
};

static const struct plrDevAPI_t *sdlInit (const struct plrDriver_t *driver, const struct plrDriverAPI_t *DriverAPI)
{
	plrDriverAPI = DriverAPI;

	PRINT("%s()\n", __FUNCTION__);

	if (SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		fprintf(stderr, "[SDL] SDL_InitSubSystem (SDL_INIT_AUDIO) failed: %s\n", SDL_GetError());
		SDL_ClearError();
		return 0;
	}

#ifdef SDL2_DEBUG
	fprintf(stderr, "[SDL] Audio drivers:\n");
	{
		int i, n;
		const char *current_driver = SDL_GetCurrentAudioDriver ();

		if (!current_driver) current_driver = "";

		n = SDL_GetNumAudioDrivers ();
		for (i=0; i < n; ++i)
		{
			const char *iter = SDL_GetAudioDriver (i);
			fprintf (stderr, "   %s %s\n", strcmp (iter, current_driver) ? "          " : "(selected)", iter);
		}

		n = SDL_GetNumAudioDevices (0);
		if (n > 0)
		{
			fprintf (stderr, "[SDL] Audio devices:\n");
			for (i=0; i < n; i++)
			{
				fprintf (stderr, "   Audio device %d: %s\n", i, SDL_GetAudioDeviceName (i, 0));
			}
		}
	}
#else
	fprintf(stderr, "[SDL] Using audio driver %s\n", SDL_GetCurrentAudioDriver());
#endif
	return &devpSDL;
}

static const struct plrDriver_t plrSDL =
{
	"devpSDL2",
	"SDL 2.x Player",
	sdlDetect,
	sdlInit,
	sdlClose
};

DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {
	.name = "devpsdl2",
	.desc = "OpenCP Player Device: SDL2 (c) 2011-'25 François Revol & Stian Skjelstad",
	.ver = DLLVERSION,
	.sortindex = 99,
	.PluginInit = sdlPluginInit,
	.PluginClose = sdlPluginClose
};
