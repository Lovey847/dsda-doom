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

#include "mux.h"
#include "doomstat.h"
#include "lprintf.h"

//////////////////////////
// Local macros
#define MAX_STREAMS 2 // Video and audio stream

////////////////////////////
// Private data
static AVFormatContext *mux_ctx; // Output file
static AVRational mux_sbase[MAX_STREAMS]; // Stream time bases
static AVStream *mux_s[MAX_STREAMS]; // File streams
static mux_stream_t mux_scnt; // Stream count

//////////////////////////
// Private functions

// Find best video encoder for output format
static enum AVCodecID GetBestVideoEncoder(AVOutputFormat *of, const char *filename) {
  // H264 is preferred, if available
  if (avformat_query_codec(of, AV_CODEC_ID_H264, 0) == 1) return AV_CODEC_ID_H264;
  else if (of->video_codec != AV_CODEC_ID_NONE) return of->video_codec;
  else return av_guess_codec(of, NULL, filename, NULL, AVMEDIA_TYPE_VIDEO);
}

// Find best audio encoder for output format
static enum AVCodecID GetBestAudioEncoder(AVOutputFormat *of, const char *filename) {
  // Ogg is preferred, if available
  if (avformat_query_codec(of, AV_CODEC_ID_VORBIS, 0) == 1) return AV_CODEC_ID_VORBIS;
  else if (of->audio_codec != AV_CODEC_ID_NONE) return of->audio_codec;
  else return av_guess_codec(of, NULL, filename, NULL, AVMEDIA_TYPE_AUDIO);
}

//////////////////////////////////////
// Public functions
dboolean MUX_Init(const char *filename, mux_codecprop_t *codecprop) {
  int ret;

  ret = avformat_alloc_output_context2(&mux_ctx, NULL, NULL, filename);
  if (ret < 0) {
    lprintf(LO_WARN, "MUX_Init: Couldn't allocate output format context!\n");
    return false;
  }

  // Put codecs in codecprop
  codecprop->vc = GetBestVideoEncoder(mux_ctx->oformat, filename);
  codecprop->ac = GetBestAudioEncoder(mux_ctx->oformat, filename);

  // Open file if it's necessary
  if (!(mux_ctx->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&mux_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
      lprintf(LO_WARN, "MUX_Init: Couldn't open output file!\n");

      MUX_Shutdown();
      return false;
    }
  }

  // Everything is initialized
  return true;
}

void MUX_Shutdown(void) {
  if (mux_ctx) {
    // Close file if necessary
    if (mux_ctx->pb && !(mux_ctx->oformat->flags & AVFMT_NOFILE))
      avio_closep(&mux_ctx->pb);

    avformat_free_context(mux_ctx);
    mux_ctx = NULL;
  }
}

dboolean MUX_AddOpt(AVCodecContext *ctx) {
  // Does this format want a global header?
  if (mux_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  return true;
}

mux_stream_t MUX_AddStream(AVCodecContext *ctx) {
  int ret;

  // Silently fail if ctx is NULL
  if (!ctx) return -1;

  if (mux_scnt >= MAX_STREAMS) {
    lprintf(LO_WARN, "MUX_AddStream: Maximum number of streams reached!\n");

    return -1;
  }

  mux_s[mux_scnt] = avformat_new_stream(mux_ctx, NULL);
  mux_s[mux_scnt]->id = mux_ctx->nb_streams-1;
  mux_sbase[mux_scnt] = mux_s[mux_scnt]->time_base = ctx->time_base;

  ret = avcodec_parameters_from_context(mux_s[mux_scnt]->codecpar, ctx);
  if (ret < 0) {
    lprintf(LO_WARN, "MUX_AddStream: Couldn't copy codec context parameters to stream %d!\n", mux_scnt);

    return -1;
  }

  return mux_scnt++;
}

dboolean MUX_WriteHeader(void) {
  int ret;

  // Log muxer settings
  av_dump_format(mux_ctx, 0, mux_ctx->url, 1);

  // Write muxer header (usually where bad setups fail)
  ret = avformat_write_header(mux_ctx, NULL);
  if (ret < 0) {
    lprintf(LO_WARN, "MUX_WriteHeader: Failed to write header!\n");

    return false;
  }

  return true;
}

dboolean MUX_WritePacket(mux_stream_t stream, AVPacket *p) {
  int ret;

  // Rescale packet timestamp
  av_packet_rescale_ts(p, mux_sbase[stream], mux_s[stream]->time_base);

  // Set packet stream index
  p->stream_index = mux_s[stream]->index;

  // Write packet to output file
  // (av_interleaved_write_frame unrefs p)
  ret = av_interleaved_write_frame(mux_ctx, p);
  if (ret < 0) {
    lprintf(LO_WARN, "MUX_WritePacket: Failed to write packet!\n");

    return false;
  }

  return true;
}

dboolean MUX_WriteTrailer(void) {
  int ret;

  ret = av_write_trailer(mux_ctx);
  if (ret < 0) {
    lprintf(LO_WARN, "MUX_Close: Failed to write trailer to file!\n");

    return false;
  }

  return true;
}
