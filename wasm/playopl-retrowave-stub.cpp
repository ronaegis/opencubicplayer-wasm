#include "config.h"

#include <string.h>

extern "C" {
#include "types.h"
#include "cpiface/cpiface.h"
}

#include "../playopl/oplRetroWave.h"

oplRetroWave::oplRetroWave(
	void(*cpiDebug)(struct cpifaceSessionAPI_t *cpifaceSession, const char *fmt, ...),
	struct cpifaceSessionAPI_t *cpifaceSession,
	const char *device,
	int rate)
{
	(void)cpiDebug;
	(void)cpifaceSession;
	(void)device;
	this->rate = rate;
	FailedToOpen = 1;
}

oplRetroWave::~oplRetroWave() = default;

void oplRetroWave::update(short *buf, int samples)
{
	if (buf && samples > 0)
	{
		memset(buf, 0, sizeof(short) * 2 * (size_t)samples);
	}
}

void oplRetroWave::write(int reg, int val)
{
	(void)reg;
	(void)val;
}

void oplRetroWave::init()
{
}
