/*
 * SDL_sound -- An abstract sound format decoding API.
 * Copyright (C) 2001  Ryan C. Gordon.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * MPGLIB decoder for SDL_sound. This is a very lightweight MP3 decoder,
 *  which is included with the SDL_sound source, so that it doesn't rely on
 *  unnecessary external libraries.
 *
 * The SMPEG decoder plays back more forms of MPEGs, and may behave better or
 *  worse under various conditions. mpglib is (apparently) more efficient than
 *  SMPEG, and, again, doesn't need an external library. You should test both
 *  decoders and use what you find works best for you.
 *
 * mpglib is part of mpg123, which can be found in its original form at:
 *   http://www.mpg123.de/
 *
 * Please see the file COPYING in the source's root directory. The included
 *  source code for mpglib falls under the LGPL, which is the same license as
 *  SDL_sound (so you can consider it a single work).
 *
 *  This file written by Ryan C. Gordon. (icculus@clutteredmind.org)
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef SOUND_SUPPORTS_MPGLIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mpglib/mpg123_sdlsound.h"
#include "mpglib/mpglib_sdlsound.h"

#include "SDL_sound.h"

#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

static int MPGLIB_init(void);
static void MPGLIB_quit(void);
static int MPGLIB_open(Sound_Sample *sample, const char *ext);
static void MPGLIB_close(Sound_Sample *sample);
static Uint32 MPGLIB_read(Sound_Sample *sample);
static int MPGLIB_rewind(Sound_Sample *sample);

static const char *extensions_mpglib[] = { "MP3", NULL };
const Sound_DecoderFunctions __Sound_DecoderFunctions_MPGLIB =
{
    {
        extensions_mpglib,
        "MP3 decoding via internal mpglib",
        "Ryan C. Gordon <icculus@clutteredmind.org>",
        "http://www.icculus.org/SDL_sound/"
    },

    MPGLIB_init,       /*   init() method */
    MPGLIB_quit,       /*   quit() method */
    MPGLIB_open,       /*   open() method */
    MPGLIB_close,      /*  close() method */
    MPGLIB_read,       /*   read() method */
    MPGLIB_rewind      /* rewind() method */
};


/* this is what we store in our internal->decoder_private field... */
typedef struct
{
    struct mpstr mp;
    Uint8 inbuf[16384];
    Uint8 outbuf[8192];
    int outleft;
    int outpos;
} mpglib_t;



static int MPGLIB_init(void)
{
    return(1);  /* always succeeds. */
} /* MPGLIB_init */


static void MPGLIB_quit(void)
{
    /* it's a no-op. */
} /* MPGLIB_quit */


static int MPGLIB_open(Sound_Sample *sample, const char *ext)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    mpglib_t *mpg = NULL;

       /*
        * If I understand things correctly, MP3 files don't really have any
        * magic header we can check for. The MP3 player is expected to just
        * pick the first thing that looks like a valid frame and start
        * playing from there.
        *
        * So here's what we do: If the caller insists that this is really
        * MP3 we'll take his word for it. Otherwise, use the same test as
        * SDL_mixer does and check if the stream starts with something that
        * looks like a frame.
        *
        * A frame begins with 11 bits of frame sync (all bits must be set),
        * followed by a two-bit MPEG Audio version ID:
        *
        *   00 - MPEG Version 2.5 (later extension of MPEG 2)
        *   01 - reserved
        *   10 - MPEG Version 2 (ISO/IEC 13818-3)
        *   11 - MPEG Version 1 (ISO/IEC 11172-3)
        *
        * Apparently we don't handle MPEG Version 2.5.
        */
    if (__Sound_strcasecmp(ext, "MP3") != 0)
    {
        Uint8 mp3_magic[2];

        if (SDL_RWread(internal->rw, mp3_magic, sizeof (mp3_magic), 1) != 1)
        {
            Sound_SetError("MP3: Could not read MP3 magic.");
            return(0);
        } /*if */

        if (mp3_magic[0] != 0xFF || (mp3_magic[1] & 0xF0) != 0xF0)
        {
            Sound_SetError("MP3: Not an MP3 stream.");
            return(0);
        } /* if */

            /* !!! FIXME: If the seek fails, we'll probably miss a frame */
        SDL_RWseek(internal->rw, -sizeof (mp3_magic), SEEK_CUR);
    } /* if */

    mpg = (mpglib_t *) malloc(sizeof (mpglib_t));
    BAIL_IF_MACRO(mpg == NULL, ERR_OUT_OF_MEMORY, 0);
    mpg->outpos = mpg->outleft = 0;
    InitMP3(&mpg->mp);

    SNDDBG(("MPGLIB: Accepting data stream.\n"));

    internal->decoder_private = mpg;
    sample->actual.rate = 44100;
    sample->actual.channels = 2;
    sample->actual.format = AUDIO_S16LSB;
    sample->flags = SOUND_SAMPLEFLAG_NONE;

    return(1); /* we'll handle this data. */
} /* MPGLIB_open */


static void MPGLIB_close(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    mpglib_t *mpg = ((mpglib_t *) internal->decoder_private);
    ExitMP3(&mpg->mp);
    free(mpg);
} /* MPGLIB_close */


static Uint32 MPGLIB_read(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    mpglib_t *mpg = ((mpglib_t *) internal->decoder_private);
    Uint32 bw = 0;
    int rc;

    while (bw < internal->buffer_size)
    {
        if (mpg->outleft > 0)
        {
            Uint16 cpysize = internal->buffer_size - bw;
            if (cpysize > mpg->outleft)
                cpysize = mpg->outleft;
            memcpy(((Uint8 *) internal->buffer) + bw,
                    mpg->outbuf + mpg->outpos, cpysize);
            bw += cpysize;
            mpg->outpos += cpysize;
            mpg->outleft -= cpysize;
            continue;
        } /* if */

        /* need to decode more from the MP3 stream... */
        mpg->outpos = 0;
        rc = decodeMP3(&mpg->mp, NULL, 0, mpg->outbuf,
                       sizeof (mpg->outbuf), &mpg->outleft);
        if (rc == MP3_ERR)
        {
            sample->flags |= SOUND_SAMPLEFLAG_ERROR;
            return(bw);
        } /* if */

        else if (rc == MP3_NEED_MORE)
        {
            rc = SDL_RWread(internal->rw, mpg->inbuf, 1, sizeof (mpg->inbuf));
            if (rc == -1)
            {
                sample->flags |= SOUND_SAMPLEFLAG_ERROR;
                return(bw);
            } /* if */
            else if (rc == 0)
            {
                sample->flags |= SOUND_SAMPLEFLAG_EOF;
                return(bw);
            } /* else if */

            rc = decodeMP3(&mpg->mp, mpg->inbuf, rc,
                           mpg->outbuf, sizeof (mpg->outbuf), &mpg->outleft);
            if (rc == MP3_ERR)
            {
                sample->flags |= SOUND_SAMPLEFLAG_ERROR;
                return(bw);
            } /* if */
        } /* else if */
    } /* while */

    return(bw);
} /* MPGLIB_read */


static int MPGLIB_rewind(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    mpglib_t *mpg = ((mpglib_t *) internal->decoder_private);
    BAIL_IF_MACRO(SDL_RWseek(internal->rw, 0, SEEK_SET) != 0, ERR_IO_ERROR, 0);

    /* this is just resetting some fields in a structure; it's very fast. */
    ExitMP3(&mpg->mp);
    InitMP3(&mpg->mp);
    mpg->outpos = mpg->outleft = 0;
    return(1);
} /* MPGLIB_rewind */


#endif /* SOUND_SUPPORTS_MPGLIB */


/* end of mpglib.c ... */

