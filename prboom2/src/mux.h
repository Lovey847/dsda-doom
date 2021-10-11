//
// Copyright(C) 2021 by Lian Ferrand
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Muxer interface, abstraction of libavformat
//

#ifndef __MUX_H__
#define __MUX_H__

#include "doomtype.h"

#ifndef HAVE_FFMPEG

#error "Don't include this file without ffmpeg libraries!"

#endif //HAVE_FFMPEG

#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

// Muxer codec properties
typedef struct mux_codecprop_s {
  enum AVCodecID vc; // Desired video encoder
  enum AVCodecID ac; // Desired audio encoder
} mux_codecprop_t;

// Muxer stream index
typedef int mux_stream_t;

// Initialize muxer, open output file and
// give properties to initialize codecs with
dboolean MUX_Init(const char *filename, mux_codecprop_t *codecprop);

// Shutdown muxer
void MUX_Shutdown(void);

// Add format specific settings to
// codec context before codec initialization
dboolean MUX_AddOpt(AVCodecContext *ctx);

// Add codec to muxer
// Returns stream index, -1 on failure
mux_stream_t MUX_AddStream(AVCodecContext *ctx);

// Write header to file
dboolean MUX_WriteHeader(void);

// Write stream packet to file
// Unrefs packet
dboolean MUX_WritePacket(mux_stream_t stream, AVPacket *p);

// Write trailer to file
dboolean MUX_WriteTrailer(void);

#endif //__MUX_H__
