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
//	Matroska container format handling,
//  for video encoding
//

#ifndef __MATROSKA_H__
#define __MATROSKA_H__

#include "doomtype.h"

#ifndef HAVE_FFMPEG

#error "Don't include this file without ffmpeg libraries!"

#endif //HAVE_FFMPEG

#include <stdio.h>
#include <libavcodec/avcodec.h>

// Write matroska header to file, and initialize muxer
// with video properties
dboolean MKV_Init(FILE *f, AVCodecContext *ctx);

// Write matroska trailer to file,
// and deinitialize muxer
void MKV_End(void);

// Write video frame to mkv file.
void MKV_WriteVideoFrame(AVPacket *p);

#endif //__MATROSKA_H__
