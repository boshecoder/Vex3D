/*
s_backend.c - sound hardware output
Copyright (C) 2009 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "port.h"

#include "common.h"
#include "sound.h"
#ifdef XASH_SDL
#include <SDL.h>
#endif
#define SAMPLE_16BIT_SHIFT		1
#define SECONDARY_BUFFER_SIZE		0x10000

/*
=======================================================================
Global variables. Must be visible to window-procedure function
so it can unlock and free the data block after it has been played.
=======================================================================
*/
convar_t		*s_primary;
convar_t		*s_khz;
dma_t			dma;

//static qboolean	snd_firsttime = true;
//static qboolean	primary_format_set;

#ifdef XASH_SDL
void SDL_SoundCallback( void* userdata, Uint8* stream, int len)
{
	int size = dma.samples << 1;
	int pos = dma.samplepos << 1;
	int wrapped = pos + len - size;

	if (wrapped < 0) {
		memcpy(stream, dma.buffer + pos, len);
		dma.samplepos += len >> 1;
	} else {
		int remaining = size - pos;
		memcpy(stream, dma.buffer + pos, remaining);
		memcpy(stream + remaining, dma.buffer, wrapped);
		dma.samplepos = wrapped >> 1;
	}
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
qboolean SNDDMA_Init( void *hInst )
{
	SDL_AudioSpec desired, obtained;
	int ret = 0;

	if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
		ret = SDL_InitSubSystem(SDL_INIT_AUDIO);
	if (ret == -1) {
		Con_Printf("Couldn't initialize SDL audio: %s\n", SDL_GetError());
		return false;
	}

	Q_memset(&desired, 0, sizeof(desired));
	switch (s_khz->integer) {
	case 48:
		desired.freq = 48000;
		break;
	case 44:
		desired.freq = 44100;
		break;
	case 22:
		desired.freq = 22050;
		break;
	default:
		desired.freq = 11025;
		break;
	}

	desired.format = AUDIO_S16LSB;
	desired.samples = 512;
	desired.channels = 2;
	desired.callback = SDL_SoundCallback;
	ret = SDL_OpenAudio(&desired, &obtained);
	if (ret == -1) {
		Con_Printf("Couldn't open SDL audio: %s\n", SDL_GetError());
		return false;
	}

	if (obtained.format != AUDIO_S16LSB) {
		Con_Printf("SDL audio format %d unsupported.\n", obtained.format);
		goto fail;
	}

	if (obtained.channels != 1 && obtained.channels != 2) {
		Con_Printf("SDL audio channels %d unsupported.\n", obtained.channels);
		goto fail;
	}

	dma.format.speed = obtained.freq;
	dma.format.channels = obtained.channels;
	dma.format.width = 2;
	dma.samples = 0x8000 * obtained.channels;
	dma.buffer = Z_Malloc(dma.samples * 2);
	dma.samplepos = 0;
	dma.sampleframes = dma.samples / dma.format.channels;

	Con_Printf("Using SDL audio driver: %s @ %d Hz\n", SDL_GetCurrentAudioDriver(), obtained.freq);

	SDL_PauseAudio(0);

	dma.initialized = true;
	return true;

fail:
	SNDDMA_Shutdown();
	return false;
}
#else
qboolean SNDDMA_Init( void *hInst )
{
	return false;
}
#endif
/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos( void )
{
	return dma.samplepos;
}

/*
==============
SNDDMA_GetSoundtime

update global soundtime
===============
*/
int SNDDMA_GetSoundtime( void )
{
	static int	buffers, oldsamplepos;
	int		samplepos, fullsamples;
	
	fullsamples = dma.samples / 2;

	// it is possible to miscount buffers
	// if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if( samplepos < oldsamplepos )
	{
		buffers++; // buffer wrapped

		if( paintedtime > 0x40000000 )
		{	
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds();
		}
	}

	oldsamplepos = samplepos;

	return (buffers * fullsamples + samplepos / 2);
}

/*
==============
SNDDMA_BeginPainting

Makes sure dma.buffer is valid
===============
*/
void SNDDMA_BeginPainting( void )
{
#ifdef XASH_SDL
	SDL_LockAudio();
#endif
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
Also unlocks the dsound buffer
===============
*/
void SNDDMA_Submit( void )
{
#ifdef XASH_SDL
	SDL_UnlockAudio();
#endif
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown( void )
{
	Con_Printf("Shutting down audio.\n");
	dma.initialized = false;
#ifdef XASH_SDL
	SDL_CloseAudio();
	if (SDL_WasInit(SDL_INIT_AUDIO != 0))
		 SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif
	if (dma.buffer) {
		 Z_Free(dma.buffer);
		 dma.buffer = NULL;
	}
}

/*
===========
S_PrintDeviceName
===========
*/
void S_PrintDeviceName( void )
{
#ifdef XASH_SDL
	Msg( "Audio: SDL (driver: %s)\n", SDL_GetCurrentAudioDriver() );
#endif
}
