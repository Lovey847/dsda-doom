/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *
 *  Copyright (C) 2011 by
 *  Nicholai Main
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *
 *---------------------------------------------------------------------
 */

#include "SDL.h"
#include "SDL_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include "i_sound.h"
#include "i_video.h"
#include "lprintf.h"
#include "i_system.h"
#include "i_capture.h"
#include "m_argv.h"
#include "dsda/palette.h"
#include "w_wad.h"

#ifdef HAVE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include "mux.h"

#endif //HAVE_FFMPEG


int capturing_video = 0;
static const char *vid_fname;


typedef struct
{ // information on a running pipe
  char command[PATH_MAX];
  FILE *f_stdin;
  FILE *f_stdout;
  FILE *f_stderr;
  SDL_Thread *outthread;
  const char *stdoutdumpname;
  SDL_Thread *errthread;
  const char *stderrdumpname;
  void *user;
} pipeinfo_t;

static pipeinfo_t soundpipe;
static pipeinfo_t videopipe;
static pipeinfo_t muxpipe;


const char *cap_soundcommand;
const char *cap_videocommand;
const char *cap_muxcommand;
const char *cap_tempfile1;
const char *cap_tempfile2;
int cap_remove_tempfiles;
int cap_fps;
int cap_frac;

// parses a command with simple printf-style replacements.

// %w video width (px)
// %h video height (px)
// %s sound rate (hz)
// %f filename passed to -viddump
// %% single percent sign
// TODO: add aspect ratio information
//
// Modified to work with SDL2 resizeable window and fullscreen desktop - DTIED
static int parsecommand (char *out, const char *in, int len)
{
  int i;

  while (*in && len > 1)
  {
    if (*in == '%')
    {
      I_UpdateRenderSize(); // Handle potential resolution scaling - DTIED
      switch (in[1])
      {
        case 'w':
          i = doom_snprintf (out, len, "%u", renderW);
          break;
        case 'h':
          i = doom_snprintf (out, len, "%u", renderH);
          break;
        case 's':
          i = doom_snprintf (out, len, "%u", snd_samplerate);
          break;
        case 'f':
          i = doom_snprintf (out, len, "%s", vid_fname);
          break;
        case 'r':
          i = doom_snprintf (out, len, "%u", cap_fps);
          break;
        case '%':
          i = doom_snprintf (out, len, "%%");
          break;
        default:
          return 0;
      }
      out += i;
      len -= i;
      in += 2;
    }
    else
    {
      *out++ = *in++;
      len--;
    }
  }
  if (*in || len < 1)
  { // out of space
    return 0;
  }
  *out = 0;
  return 1;
}

// FFmpeg library
#ifdef HAVE_FFMPEG

static AVPacket *vid_packet = NULL;

static mux_stream_t vid_stream;
static const AVCodec *vid_codec = NULL;
static AVCodecContext *vid_ctx = NULL;
static AVFrame *vid_frame = NULL;

// Pixel format of video, either
// AV_PIX_FMT_NV12 or AV_PIX_FMT_YUV420P
// NV12 is preferred
static enum AVPixelFormat vid_fmt = AV_PIX_FMT_NONE;

static int vid_curframe;

static mux_stream_t snd_stream;
static const AVCodec *snd_codec = NULL;
static AVCodecContext *snd_ctx = NULL;
static AVFrame *snd_frame = NULL;

// Audio sample format
// We support S16 (fastest), S16P, and FLTP (slowest)
static enum AVSampleFormat snd_fmt = AV_SAMPLE_FMT_NONE;

// Sample write callback
static void (*snd_write)(const short *samples, size_t ptr, size_t numsamples);

static int snd_cursample;

// playpal in YCbCr (BT.709)
static byte *vid_playpal = NULL;

// Sample writing prototypes
static void I_WriteS16Samples(const short *samples, size_t ptr, size_t numsamples);
static void I_WriteS16PSamples(const short *samples, size_t ptr, size_t numsamples);
static void I_WriteFLTPSamples(const short *samples, size_t ptr, size_t numsamples);

// Encode vid_frame into vid_file with vid_codec
static dboolean I_EncodeFrame(AVCodecContext *ctx, mux_stream_t stream, AVFrame *f) {
  int ret;

  // Send frame to encoder
  ret = avcodec_send_frame(ctx, f);
  if (ret < 0) {
    lprintf(LO_WARN, "I_EncodeFrame: Couldn't send frame to %s!\n", vid_codec->name);
    return false;
  }

  // Flush the encoder's output
  for (;;) {
    ret = avcodec_receive_packet(ctx, vid_packet);

    // If the encoder's been flushed, break out of loop
    if ((ret == AVERROR(EAGAIN)) || (ret == AVERROR_EOF)) break;

    // Fatal error occurred
    if (ret < 0) {
      lprintf(LO_WARN, "I_EncodeFrame: An error occurred while flushing the encoder!\n");
      return false;
    }

    // Write packet to muxer
    MUX_WritePacket(stream, vid_packet); // unrefs packet
  }

  return true;
}

// Allocate vid_playpal, which is the playpal in YCbCr
static void I_AllocYUVPlaypal(void) {
  dsda_playpal_t *playpaldata = dsda_PlayPalData();
  const byte *playpal = V_GetPlaypal();
  byte r, g, b;
  int playpalsize = W_LumpLength(W_GetNumForName(playpaldata->lump_name));
  int i;

  vid_playpal = Z_Malloc(3 * playpalsize, PU_STATIC, NULL);

  // Parse playpal into vid_playpal
  for (i = playpalsize*3; i > 0;) {
    i -= 3;

    // Read RGB palette color into YCbCr palette
    // Assume r, g and b are normalized from 0 to 1
    //
    // BT.709
    // Kr = 0.2126
    // Kg = 0.7152
    // Kb = 0.0722
    //
    // Y = 16 + ((r*Kr)*219) + ((g*Kg)*219) + ((b*Kb)*219)
    //
    // Assume Y is normalized from 0 to 1
    //
    // Cb = 128 + ((b-Y)/(1-Kb))*128
    // Cr = 128 + ((r-Y)/(1-Kr))*128

    r = playpal[i];
    g = playpal[i+1];
    b = playpal[i+2];

    // 47 = Kr*219 * 256 / 255
    // 157 = Kg*219 * 256 / 255
    // 16 = Kb*219 * 256 / 255
    vid_playpal[i] = 16 + ((r*47)>>8) + ((g*157)>>8) + ((b*16)>>8);

    // 26 = Kr/(1-Kb) * 112 * 256 / 255
    // 87 = Kg/(1-Kb) * 112 * 256 / 255
    vid_playpal[i+1] = 128 - ((r*26)>>8) - ((g*87)>>8) + ((b*112)>>8);

    // 102 = Kg/(1-Kr) * 112 * 256 / 255
    // 10 = Kb/(1-Kr) * 112 * 256 / 255
    vid_playpal[i+2] = 128 + ((r*112)>>8) - ((g*102)>>8) - ((b*10)>>8);
  }
}

// Open a preferrable codec, if available
// If none are available, try to open the default codec
static dboolean I_OpenCodec(dboolean (*trycodec)(enum AVCodecID), const AVOutputFormat *ofmt,
                            const enum AVCodecID *pref, size_t preflen, enum AVCodecID def)
{
  const enum AVCodecID *i;

  for (i = pref; i < pref+preflen; ++i) {
    // Is this codec not supported by the format?
    if (avformat_query_codec(ofmt, *i, 0) != 1) continue;

    // Can we initialize this codec?
    if (trycodec(*i)) return true;
  }

  // If no preferred codec could be initialized, try default codec
  if (trycodec(def)) return true;

  // If we still didn't initialize, error out
  lprintf(LO_WARN, "I_OpenCodec: Couldn't initialize any codec!\n");

  return false;
}

// Shutdown video encoder context
static void I_CloseVideo(void) {
  if (vid_ctx) avcodec_free_context(&vid_ctx);
  if (vid_frame) av_frame_free(&vid_frame);

  vid_codec = NULL;
  vid_fmt = AV_PIX_FMT_NONE;
}

// Attempt to open specific video encoder
static dboolean I_TryVideoCodec(enum AVCodecID c) {
  int arg, ret;
  AVDictionary *opts = NULL;
  const enum AVPixelFormat *fmt;

  // Find encoder
  vid_codec = avcodec_find_encoder(c);
  if (!vid_codec) {
    lprintf(LO_WARN, "I_TryVideoCodec: Cannot find encoder for %s!\n", avcodec_get_name(c));

    I_CloseVideo();
    return false;
  }

  // Find pixel format for encoder
  for (fmt = vid_codec->pix_fmts; *fmt != -1; ++fmt) {
    if (*fmt == AV_PIX_FMT_NV12) {
      vid_fmt = AV_PIX_FMT_NV12;
      break;
    } else if (*fmt == AV_PIX_FMT_YUV420P) vid_fmt = AV_PIX_FMT_YUV420P;
  }

  if (vid_fmt == AV_PIX_FMT_NONE) {
    lprintf(LO_WARN, "I_TryVideoCodec: Encoder doesn't support YUV420P!\n");

    I_CloseVideo();
    return false;
  }

  // Allocate encoder context
  vid_ctx = avcodec_alloc_context3(vid_codec);
  if (!vid_ctx) {
    lprintf(LO_WARN, "I_TryVideoCodec: Couldn't allocate encoder context!\n");

    I_CloseVideo();
    return false;
  }

  // Open encoder
  vid_ctx->width = SCREENWIDTH;
  vid_ctx->height = SCREENHEIGHT;
  vid_ctx->time_base.num = 1; vid_ctx->time_base.den = cap_fps;
  vid_ctx->framerate.num = cap_fps; vid_ctx->framerate.den = 1;
  vid_ctx->gop_size = cap_fps/2;
  vid_ctx->colorspace = AVCOL_SPC_BT709;
  vid_ctx->color_trc = AVCOL_TRC_BT709;
  vid_ctx->color_primaries = AVCOL_PRI_BT709;
  vid_ctx->color_range = AVCOL_RANGE_MPEG;
  vid_ctx->pix_fmt = vid_fmt;

  // Common encoder private options
  if (!strcmp(vid_codec->name, "libx264")) {
    av_dict_set(&opts, "profile", "baseline", 0);
    av_dict_set(&opts, "preset", "ultrafast", 0);
    av_dict_set(&opts, "tune", "zerolatency", 0);
    av_dict_set(&opts, "thread_type", "frame", 0);
  }

  // Set video bitrate
  if ((arg = M_CheckParm("-vb")) && (arg < myargc-1))
    vid_ctx->bit_rate = atoi(myargv[arg+1])*1024*1024;

  // Add muxer-specific options
  if (!MUX_AddOpt(vid_ctx)) {
    lprintf(LO_WARN, "I_TryVideoCodec: Couldn't write muxer-specific options!\n");

    I_CloseVideo();
    return false;
  }

  // Open codec
  ret = avcodec_open2(vid_ctx, vid_codec, opts ? &opts : NULL);
  if (ret < 0) {
    lprintf(LO_WARN, "I_TryVideoCodec: Couldn't initialize codec context!\n");

    I_CloseVideo();
    return false;
  }

  if (opts) av_dict_free(&opts);

  return true;
}

// Open video encoder context
static dboolean I_OpenVideoContext(const mux_codecprop_t *prop) {
  // List of preferrable video encoders
  static const enum AVCodecID preferred[] = {
    AV_CODEC_ID_H264
  };

  int ret;

  // Open any available video codec
  if (!I_OpenCodec(I_TryVideoCodec, prop->ofmt,
                   preferred, sizeof(preferred)/sizeof(*preferred),
                   prop->vc))
  {
    lprintf(LO_WARN, "I_OpenVideoContext: Couldn't initialize any encoder! (use -nodraw to only dump audio)\n");

    return false;
  }

  // Allocate video frame
  vid_frame = av_frame_alloc();
  if (!vid_frame) {
    lprintf(LO_WARN, "I_OpenVideoContext: Couldn't allocate video frame!\n");

    return false;
  }

  vid_frame->format = vid_fmt;
  vid_frame->width = SCREENWIDTH;
  vid_frame->height = SCREENHEIGHT;

  ret = av_frame_get_buffer(vid_frame, 0);
  if (ret < 0) {
    lprintf(LO_WARN, "I_OpenVideoContext: Couldn't get video frame buffers!\n");

    return false;
  }

  return true;
}

// Shutdown audio encoder context
static void I_CloseAudio(void) {
  if (snd_ctx) avcodec_free_context(&snd_ctx);
  if (snd_frame) av_frame_free(&snd_frame);

  snd_codec = NULL;
  snd_fmt = AV_SAMPLE_FMT_NONE;
}

// Attempt to open specific audio encoder
static dboolean I_TryAudioCodec(enum AVCodecID c) {
  int arg, ret;
  AVDictionary *opts = NULL;
  const enum AVSampleFormat *fmt;

  // Find encoder
  snd_codec = avcodec_find_encoder(c);
  if (!snd_codec) {
    lprintf(LO_WARN, "I_TryAudioCodec: Couldn't find encoder for %s!\n", avcodec_get_name(c));

    I_CloseAudio();
    return false;
  }

  // Get sample format
  for (fmt = snd_codec->sample_fmts; *fmt >= 0; ++fmt) {
    switch (*fmt) {
    case AV_SAMPLE_FMT_S16:
      snd_fmt = AV_SAMPLE_FMT_S16;
      snd_write = I_WriteS16Samples;

      break;
    case AV_SAMPLE_FMT_S16P:
      if (snd_fmt != AV_SAMPLE_FMT_S16) {
        snd_fmt = AV_SAMPLE_FMT_S16P;
        snd_write = I_WriteS16PSamples;
      }

      break;
    case AV_SAMPLE_FMT_FLTP:
      if (snd_fmt == AV_SAMPLE_FMT_NONE) {
        snd_fmt = AV_SAMPLE_FMT_FLTP;
        snd_write = I_WriteFLTPSamples;
      }

      break;
    }
  }

  // Did we find a supported audio format?
  if (snd_fmt == AV_SAMPLE_FMT_NONE) {
    lprintf(LO_WARN, "I_TryAudioCodec: Encoder doesn't support s16!\n");

    I_CloseAudio();
    return false;
  }

  // Allocate encoder context
  snd_ctx = avcodec_alloc_context3(snd_codec);
  if (!snd_ctx) {
    lprintf(LO_WARN, "I_TryAudioCodec: Couldn't allocate encoder context!\n");

    I_CloseAudio();
    return false;
  }

  // Open encoder
  snd_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
  snd_ctx->sample_rate = snd_samplerate;
  snd_ctx->time_base.num = 1; snd_ctx->time_base.den = snd_samplerate;
  snd_ctx->channels = 2;
  snd_ctx->sample_fmt = snd_fmt;

  // Set audio bitrate
  if ((arg = M_CheckParm("-ab")) && (arg < myargc-1))
    snd_ctx->bit_rate = atoi(myargv[arg+1])*1024;

  // Add muxer-specific options
  if (!MUX_AddOpt(snd_ctx)) {
    lprintf(LO_WARN, "I_TryAudioCodec: Couldn't add muxer-specific options to codec!\n");

    I_CloseAudio();
    return false;
  }

  // Open codec
  ret = avcodec_open2(snd_ctx, snd_codec, NULL);
  if (ret < 0) {
    lprintf(LO_WARN, "I_TryAudioCodec: Couldn't initialize codec!\n");

    I_CloseAudio();
    return false;
  }

  return true;
}

// Open audio encoder context
static dboolean I_OpenAudioContext(const mux_codecprop_t *prop) {
  // List of preferrable audio encoders
  static const enum AVCodecID preferred[] = {
    AV_CODEC_ID_OPUS, AV_CODEC_ID_MP3
  };

  int ret;

  // Open any available audio codec
  if (!I_OpenCodec(I_TryAudioCodec, prop->ofmt,
                   preferred, sizeof(preferred)/sizeof(*preferred),
                   prop->ac))
  {
    lprintf(LO_WARN, "I_OpenAudioContext: Couldn't initialize any encoder! (use -nosound to only dump video)\n");

    return false;
  }

  // Allocate audio frame
  snd_frame = av_frame_alloc();
  if (!snd_frame) {
    lprintf(LO_WARN, "I_OpenAudioContext: Couldn't allocate audio frame!\n");

    return false;
  }

  snd_frame->format = snd_fmt;
  snd_frame->nb_samples = snd_ctx->frame_size;
  snd_frame->channel_layout = AV_CH_LAYOUT_STEREO;
  snd_frame->sample_rate = snd_samplerate;

  ret = av_frame_get_buffer(snd_frame, 0);
  if (ret < 0) {
    lprintf(LO_WARN, "I_OpenAudioContext: Couldn't get buffers for audio frame!\n");

    return false;
  }

  return true;
}

void I_CapturePrep(const char *fn) {
  int arg;
  int ret;
  mux_codecprop_t prop;

  vid_fname = fn;

  // We can't record opengl in this mode
  if (V_IsOpenGLMode()) {
    lprintf(LO_WARN, "I_CapturePrep: Cannot record in OpenGL mode!\n");
    capturing_video = 0;

    I_CaptureFinish();
    return;
  }

  // We can only record with even width & height
  if ((SCREENWIDTH&1) || (SCREENHEIGHT&1)) {
    lprintf(LO_WARN, "I_CapturePrep: Can only record with even width&height!\n");
    capturing_video = 0;

    I_CaptureFinish();
    return;
  }

  // Allocate vid_playpal
  I_AllocYUVPlaypal();

  // Allocate packet, used when encoding
  vid_packet = av_packet_alloc();
  if (!vid_packet) {
    lprintf(LO_WARN, "I_CapturePrep: Couldn't allocate packet!\n");
    capturing_video = 0;

    I_CaptureFinish();
    return;
  }

  // Initialize muxer
  if (!MUX_Init(vid_fname, &prop)) {
    lprintf(LO_WARN, "I_CapturePrep: Couldn't initialize muxer!\n");
    capturing_video = 0;

    I_CaptureFinish();
    return;
  }

  // Initialize video encoder context
  if (!M_CheckParm("-nodraw") && !I_OpenVideoContext(&prop)) {
    lprintf(LO_WARN, "I_CapturePrep: Couldn't open video encoder context!\n");
    capturing_video = 0;

    I_CaptureFinish();
    return;
  }

  // Initialize audio encoder context
  if (!M_CheckParm("-nosound") && !I_OpenAudioContext(&prop)) {
    lprintf(LO_WARN, "I_CapturePrep: Couldn't open audio encoder context!\n");
    capturing_video = 0;

    I_CaptureFinish();
    return;
  }

  // Add video stream to muxer
  vid_stream = MUX_AddStream(vid_ctx);

  // Add audio stream to muxer
  snd_stream = MUX_AddStream(snd_ctx);

  // Write file header
  if (!MUX_WriteHeader()) {
    lprintf(LO_WARN, "I_CapturePrep: Couldn't initialize muxer!\n");
    capturing_video = 0;

    I_CaptureFinish();
    return;
  }

  // Initialization done
  I_SetSoundCap();

  lprintf(LO_INFO, "I_CapturePrep: Video capture initiated\n");
  capturing_video = 1;
  vid_curframe = 0;
  I_AtExit(I_CaptureFinish, true);
}

// Free anything that needs to be freed
void I_CaptureFinish(void) {
  // Free vid_playpal
  if (vid_playpal) Z_Free(vid_playpal);

  // If we're recording, flush encoder output and write trailer to file
  if (capturing_video) {
    if (vid_ctx) I_EncodeFrame(vid_ctx, vid_stream, NULL);

    // TODO: The last audio frame may contain game audio!
    if (snd_ctx) I_EncodeFrame(snd_ctx, snd_stream, NULL);

    MUX_WriteTrailer();
  }

  // Shutdown muxer
  MUX_Shutdown();

  if (vid_ctx) avcodec_free_context(&vid_ctx);
  if (vid_frame) av_frame_free(&vid_frame);

  if (snd_ctx) avcodec_free_context(&snd_ctx);
  if (snd_frame) av_frame_free(&snd_frame);

  if (vid_packet) av_packet_free(&vid_packet);

  capturing_video = 0;
}

// Get the average chrominance of a 2x2 pixel region
static void I_AverageChrominance(byte *cbp, byte *crp, byte *pixels) {
  int cb, cr;

  cb = vid_playpal[pixels[0]*3+1];
  cr = vid_playpal[pixels[0]*3+2];

  cb += vid_playpal[pixels[1]*3+1];
  cr += vid_playpal[pixels[1]*3+2];

  cb += vid_playpal[pixels[screens[0].pitch]*3+1];
  cr += vid_playpal[pixels[screens[0].pitch]*3+2];

  cb += vid_playpal[pixels[screens[0].pitch+1]*3+1];
  cr += vid_playpal[pixels[screens[0].pitch+1]*3+2];

  *cbp = cb>>2;
  *crp = cr>>2;
}

// Write NV12 chrominance plane
static void I_WriteNV12Chroma(void) {
  byte *ptr;
  int y, x;

  ptr = vid_frame->data[1];
  for (y = 0; y < SCREENHEIGHT; y += 2) {
    for (x = 0; x < SCREENWIDTH; x += 2) {
      I_AverageChrominance(ptr, ptr+1, screens[0].data + y*screens[0].pitch + x);
      ptr += 2;
    }
  }
}

// Write YUV420P chrominance planes
static void I_WriteYUVChroma(void) {
  byte *ptr, *crptr;
  int y, x;

  ptr = vid_frame->data[1];
  crptr = vid_frame->data[2];
  for (y = 0; y < SCREENHEIGHT; y += 2) {
    for (x = 0; x < SCREENWIDTH; x += 2) {
      I_AverageChrominance(ptr, crptr, screens[0].data + y*screens[0].pitch + x);
      ++ptr;
      ++crptr;
    }
  }
}

// Encode a single frame of video
static void I_EncodeVideoFrame(void) {
  int ret;
  int x, y;
  byte *ptr;

  // Make sure frame is writable
  ret = av_frame_make_writable(vid_frame);
  if (ret < 0) {
    lprintf(LO_WARN, "I_EncodeVideoFrame: Couldn't make video frame writable!\n");
    return;
  }

  // TODO: Hook into I_SetPalette to get the current palette!
  //       For now, use the first playpal

  // Write luminance
  ptr = vid_frame->data[0];
  for (y = 0; y < SCREENHEIGHT; ++y) {
    for (x = 0; x < SCREENWIDTH; ++x)
      *ptr++ = vid_playpal[screens[0].data[y*screens[0].pitch + x]*3];
  }

  // Write chrominance
  if (vid_fmt == AV_PIX_FMT_NV12)
    I_WriteNV12Chroma();
  else // YUV420P
    I_WriteYUVChroma();

  vid_frame->pts = vid_curframe++;

  if (!I_EncodeFrame(vid_ctx, vid_stream, vid_frame))
    lprintf(LO_WARN, "I_EncodeVideoFrame: Couldn't encode frame %d!\n", vid_curframe);
}

// Split interleaved samples into 2 buffers
static void I_SplitSamples(const short *samples, size_t numsamples,
                           short *left, short *right)
{
  const short *i, *end;

  for (i = samples, end = i+numsamples*2;
       i != end; i += 2)
  {
    *left++ = i[0];
    *right++ = i[1];
  }
}

// Convert interleaved s16 samples to planar flt samples
static void I_CvtSamples(const short *samples, size_t numsamples,
                         float *left, float *right)
{
  const short *i, *end;

  for (i = samples, end = i+numsamples*2;
       i != end; i += 2)
  {
    // Normalize range
    *left++ = ((float)i[0]+0.5f)*(1.f/32767.5f);
    *right++ = ((float)i[1]+0.5f)*(1.f/32767.5f);
  }
}

// Write samples in s16 format
static void I_WriteS16Samples(const short *samples, size_t ptr, size_t numsamples) {
  // snd_frame->data[0] is a uint8_t*
  memcpy(snd_frame->data[0] + ptr*4,
         samples,
         numsamples*4);
}

// Write samples in s16p format
static void I_WriteS16PSamples(const short *samples, size_t ptr, size_t numsamples) {
  I_SplitSamples(samples, numsamples, (short*)snd_frame->data[0] + ptr, (short*)snd_frame->data[1] + ptr);
}

// Write samples in fltp format
static void I_WriteFLTPSamples(const short *samples, size_t ptr, size_t numsamples) {
  I_CvtSamples(samples, numsamples, (float*)snd_frame->data[0] + ptr, (float*)snd_frame->data[1] + ptr);
}

// Encode a game tic of audio
static void I_EncodeAudioFrame(void) {
  static size_t bufptr = 0; // Pointer into sample buffer

  // Track fraction of samples, add extra sample everytime this is >= cap_fps
  static int carry = 0;

  int ret;
  size_t numsamples;
  const short *samplebuf;

  // Make sure frame is writable
  ret = av_frame_make_writable(snd_frame);
  if (ret < 0) {
    lprintf(LO_WARN, "I_EncodeAudioFrame: Couldn't make audio frame writable!\n");
    return;
  }

  // Get number of samples to read
  numsamples = snd_samplerate/cap_fps;
  carry += snd_samplerate%cap_fps;

  if (carry >= cap_fps) {
    carry -= cap_fps;
    ++numsamples;
  }

  // Get audio samples
  samplebuf = (const short*)I_GrabSound(numsamples);

  // Is the sample buffer full?
  while (bufptr + numsamples >= snd_frame->nb_samples) {
    const size_t samples = snd_frame->nb_samples-bufptr;

    numsamples -= samples;

    snd_write(samplebuf, bufptr, samples);
    bufptr = 0;
    samplebuf += samples*2;

    snd_frame->pts = snd_cursample;
    snd_cursample += snd_frame->nb_samples;

    if (!I_EncodeFrame(snd_ctx, snd_stream, snd_frame))
      lprintf(LO_WARN, "I_EncodeAudioFrame: Error encoding frame!\n");

    // If that was all the samples, return
    if (!numsamples) return;
  }

  // Store samples in buffer, waiting for next encode
  snd_write(samplebuf, bufptr, numsamples);
  bufptr += numsamples;
}

// Encode a single frame of video
void I_CaptureFrame(void) {
  if (!capturing_video) return;

  // Encode video frame
  if (vid_ctx) I_EncodeVideoFrame();

  // Encode audio frame
  if (snd_ctx) I_EncodeAudioFrame();
}

// FFmpeg process
#else //HAVE_FFMPEG

// popen3() implementation -
// starts a child process

// user is a pointer to implementation defined extra data
static int my_popen3 (pipeinfo_t *p); // 1 on success
// close waits on process
static void my_pclose3 (pipeinfo_t *p);


#ifdef _WIN32
// direct winapi implementation
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <io.h>

typedef struct
{
  HANDLE proc;
  HANDLE thread;
} puser_t;

// extra pointer is used to hold process id to wait on to close
// NB: stdin is opened as "wb", stdout, stderr as "r"
static int my_popen3 (pipeinfo_t *p)
{
  FILE *fin = NULL;
  FILE *fout = NULL;
  FILE *ferr = NULL;
  HANDLE child_hin = INVALID_HANDLE_VALUE;
  HANDLE child_hout = INVALID_HANDLE_VALUE;
  HANDLE child_herr = INVALID_HANDLE_VALUE;
  HANDLE parent_hin = INVALID_HANDLE_VALUE;
  HANDLE parent_hout = INVALID_HANDLE_VALUE;
  HANDLE parent_herr = INVALID_HANDLE_VALUE;


  puser_t *puser = NULL;


  PROCESS_INFORMATION piProcInfo;
  STARTUPINFO siStartInfo;
  SECURITY_ATTRIBUTES sa;

  puser = malloc (sizeof (puser_t));
  if (!puser)
    return 0;

  puser->proc = INVALID_HANDLE_VALUE;
  puser->thread = INVALID_HANDLE_VALUE;


  // make the pipes

  sa.nLength = sizeof (sa);
  sa.bInheritHandle = 1;
  sa.lpSecurityDescriptor = NULL;
  if (!CreatePipe (&child_hin, &parent_hin, &sa, 0))
    goto fail;
  if (!CreatePipe (&parent_hout, &child_hout, &sa, 0))
    goto fail;
  if (!CreatePipe (&parent_herr, &child_herr, &sa, 0))
    goto fail;


  // very important
  if (!SetHandleInformation (parent_hin, HANDLE_FLAG_INHERIT, 0))
    goto fail;
  if (!SetHandleInformation (parent_hout, HANDLE_FLAG_INHERIT, 0))
    goto fail;
  if (!SetHandleInformation (parent_herr, HANDLE_FLAG_INHERIT, 0))
    goto fail;




  // start the child process

  ZeroMemory (&siStartInfo, sizeof (STARTUPINFO));
  siStartInfo.cb         = sizeof (STARTUPINFO);
  siStartInfo.hStdInput  = child_hin;
  siStartInfo.hStdOutput = child_hout;
  siStartInfo.hStdError  = child_herr;
  siStartInfo.dwFlags    = STARTF_USESTDHANDLES;

  if (!CreateProcess(NULL,// application name
       (LPTSTR)p->command,// command line
       NULL,              // process security attributes
       NULL,              // primary thread security attributes
       TRUE,              // handles are inherited
       DETACHED_PROCESS,  // creation flags
       NULL,              // use parent's environment
       NULL,              // use parent's current directory
       &siStartInfo,      // STARTUPINFO pointer
       &piProcInfo))      // receives PROCESS_INFORMATION
  {
    goto fail;
  }



  puser->proc = piProcInfo.hProcess;
  puser->thread = piProcInfo.hThread;


  if (NULL == (fin = _fdopen (_open_osfhandle ((intptr_t) parent_hin, 0), "wb")))
    goto fail;
  if (NULL == (fout = _fdopen (_open_osfhandle ((intptr_t) parent_hout, 0), "r")))
    goto fail;
  if (NULL == (ferr = _fdopen (_open_osfhandle ((intptr_t) parent_herr, 0), "r")))
    goto fail;
  // after fdopen(osf()), we don't need to keep track of parent handles anymore
  // fclose on the FILE struct will automatically free them


  p->user = puser;
  p->f_stdin = fin;
  p->f_stdout = fout;
  p->f_stderr = ferr;

  CloseHandle (child_hin);
  CloseHandle (child_hout);
  CloseHandle (child_herr);

  return 1;

  fail:
  if (fin)
    fclose (fin);
  if (fout)
    fclose (fout);
  if (ferr)
    fclose (ferr);

  if (puser->proc)
    CloseHandle (puser->proc);
  if (puser->thread)
    CloseHandle (puser->thread);

  if (child_hin != INVALID_HANDLE_VALUE)
    CloseHandle (child_hin);
  if (child_hout != INVALID_HANDLE_VALUE)
    CloseHandle (child_hout);
  if (child_herr != INVALID_HANDLE_VALUE)
    CloseHandle (child_herr);
  if (parent_hin != INVALID_HANDLE_VALUE)
    CloseHandle (parent_hin);
  if (parent_hout != INVALID_HANDLE_VALUE)
    CloseHandle (parent_hout);
  if (parent_herr != INVALID_HANDLE_VALUE)
    CloseHandle (parent_herr);

  free (puser);

  return 0;


}

static void my_pclose3 (pipeinfo_t *p)
{
  puser_t *puser = (puser_t *) p->user;

  if (!p->f_stdin || !p->f_stdout || !p->f_stderr || !puser)
    return;

  fclose (p->f_stdin);
  //fclose (p->f_stdout); // these are closed elsewhere
  //fclose (p->f_stderr);

  WaitForSingleObject (puser->proc, INFINITE);

  CloseHandle (puser->proc);
  CloseHandle (puser->thread);
  free (puser);
}

#else // _WIN32
// posix implementation
// not tested
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


typedef struct
{
  int pid;
} puser_t;


static int my_popen3 (pipeinfo_t *p)
{
  FILE *fin = NULL;
  FILE *fout = NULL;
  FILE *ferr = NULL;
  int child_hin = -1;
  int child_hout = -1;
  int child_herr = -1;
  int parent_hin = -1;
  int parent_hout = -1;
  int parent_herr = -1;

  int scratch[2];

  int pid;

  puser_t *puser = NULL;

  puser = malloc (sizeof (puser_t));
  if (!puser)
    return 0;



  // make the pipes
  if (pipe (scratch))
    goto fail;
  child_hin = scratch[0];
  parent_hin = scratch[1];
  if (pipe (scratch))
    goto fail;
  parent_hout = scratch[0];
  child_hout = scratch[1];
  if (pipe (scratch))
    goto fail;
  parent_herr = scratch[0];
  child_herr = scratch[1];

  pid = fork ();

  if (pid == -1)
    goto fail;
  if (pid == 0)
  {
    dup2 (child_hin, STDIN_FILENO);
    dup2 (child_hout, STDOUT_FILENO);
    dup2 (child_herr, STDERR_FILENO);

    close (parent_hin);
    close (parent_hout);
    close (parent_herr);

    // does this work? otherwise we have to parse cmd into an **argv style array
    execl ("/bin/sh", "sh", "-c", p->command, NULL);
    // exit forked process if command failed
    _exit (0);
  }

  if (NULL == (fin = fdopen (parent_hin, "wb")))
    goto fail;
  if (NULL == (fout = fdopen (parent_hout, "r")))
    goto fail;
  if (NULL == (ferr = fdopen (parent_herr, "r")))
    goto fail;

  close (child_hin);
  close (child_hout);
  close (child_herr);

  puser->pid = pid;

  p->user = puser;
  p->f_stdin = fin;
  p->f_stdout = fout;
  p->f_stderr = ferr;
  return 1;

  fail:
  if (fin)
    fclose (fin);
  if (fout)
    fclose (fout);
  if (ferr)
    fclose (ferr);

  close (parent_hin);
  close (parent_hout);
  close (parent_herr);
  close (child_hin);
  close (child_hout);
  close (child_herr);

  free (puser);
  return 0;

}


static void my_pclose3 (pipeinfo_t *p)
{
  puser_t *puser = (puser_t *) p->user;

  int s;

  if (!p->f_stdin || !p->f_stdout || !p->f_stderr || !puser)
    return;

  fclose (p->f_stdin);
  //fclose (p->f_stdout); // these are closed elsewhere
  //fclose (p->f_stderr);

  waitpid (puser->pid, &s, 0);

  free (puser);
}


#endif // _WIN32

typedef struct
{
  FILE *fin;
  const char *fn;
} threaddata_t;


static int threadstdoutproc (void *data)
{ // simple thread proc dumps stdout
  // not terribly fast
  int c;

  pipeinfo_t *p = (pipeinfo_t *) data;

  FILE *f = fopen (p->stdoutdumpname, "w");

  if (!f || !p->f_stdout)
    return 0;

  while ((c = fgetc (p->f_stdout)) != EOF)
    fputc (c, f);

  fclose (f);
  fclose (p->f_stdout);
  return 1;
}

static int threadstderrproc (void *data)
{ // simple thread proc dumps stderr
  // not terribly fast
  int c;

  pipeinfo_t *p = (pipeinfo_t *) data;

  FILE *f = fopen (p->stderrdumpname, "w");

  if (!f || !p->f_stderr)
    return 0;

  while ((c = fgetc (p->f_stderr)) != EOF)
    fputc (c, f);

  fclose (f);
  fclose (p->f_stderr);
  return 1;
}


// init and open sound, video pipes
// fn is filename passed from command line, typically final output file
void I_CapturePrep (const char *fn)
{
  vid_fname = fn;

  if (!parsecommand (soundpipe.command, cap_soundcommand, sizeof(soundpipe.command)))
  {
    lprintf (LO_ERROR, "I_CapturePrep: malformed command %s\n", cap_soundcommand);
    capturing_video = 0;
    return;
  }
  if (!parsecommand (videopipe.command, cap_videocommand, sizeof(videopipe.command)))
  {
    lprintf (LO_ERROR, "I_CapturePrep: malformed command %s\n", cap_videocommand);
    capturing_video = 0;
    return;
  }
  if (!parsecommand (muxpipe.command, cap_muxcommand, sizeof(muxpipe.command)))
  {
    lprintf (LO_ERROR, "I_CapturePrep: malformed command %s\n", cap_muxcommand);
    capturing_video = 0;
    return;
  }

  lprintf (LO_INFO, "I_CapturePrep: opening pipe \"%s\"\n", soundpipe.command);
  if (!my_popen3 (&soundpipe))
  {
    lprintf (LO_ERROR, "I_CapturePrep: sound pipe failed\n");
    capturing_video = 0;
    return;
  }
  lprintf (LO_INFO, "I_CapturePrep: opening pipe \"%s\"\n", videopipe.command);
  if (!my_popen3 (&videopipe))
  {
    lprintf (LO_ERROR, "I_CapturePrep: video pipe failed\n");
    my_pclose3 (&soundpipe);
    capturing_video = 0;
    return;
  }
  I_SetSoundCap ();
  lprintf (LO_INFO, "I_CapturePrep: video capture started\n");
  capturing_video = 1;

  // start reader threads
  soundpipe.stdoutdumpname = "sound_stdout.txt";
  soundpipe.stderrdumpname = "sound_stderr.txt";
  soundpipe.outthread = SDL_CreateThread (threadstdoutproc, "soundpipe.outthread", &soundpipe);
  soundpipe.errthread = SDL_CreateThread (threadstderrproc, "soundpipe.errthread", &soundpipe);
  videopipe.stdoutdumpname = "video_stdout.txt";
  videopipe.stderrdumpname = "video_stderr.txt";
  videopipe.outthread = SDL_CreateThread (threadstdoutproc, "videopipe.outthread", &videopipe);
  videopipe.errthread = SDL_CreateThread (threadstderrproc, "videopipe.errthread", &videopipe);

  I_AtExit (I_CaptureFinish, true);
}



// capture a single frame of video (and corresponding audio length)
// and send it to pipes
// Modified to work with SDL2 resizeable window and fullscreen desktop - DTIED
void I_CaptureFrame (void)
{
  unsigned char *snd;
  unsigned char *vid;
  static int partsof35 = 0; // correct for sync when samplerate % 35 != 0
  int nsampreq;

  if (!capturing_video)
    return;

  nsampreq = snd_samplerate / cap_fps;
  partsof35 += snd_samplerate % cap_fps;
  if (partsof35 >= cap_fps)
  {
    partsof35 -= cap_fps;
    nsampreq++;
  }

  snd = I_GrabSound (nsampreq);
  if (snd)
  {
    if (fwrite (snd, nsampreq * 4, 1, soundpipe.f_stdin) != 1)
      lprintf(LO_WARN, "I_CaptureFrame: error writing soundpipe.\n");
    //free (snd); // static buffer
  }
  vid = I_GrabScreen ();
  if (vid)
  {
    if (fwrite (vid, renderW * renderH * 3, 1, videopipe.f_stdin) != 1)
      lprintf(LO_WARN, "I_CaptureFrame: error writing videopipe.\n");
    //free (vid); // static buffer
  }

}


// close pipes, call muxcommand, finalize
void I_CaptureFinish (void)
{
  int s;

  if (!capturing_video)
    return;
  capturing_video = 0;

  // on linux, we have to close videopipe first, because it has a copy of the write
  // end of soundpipe_stdin (so that stream will never see EOF).
  // is there a better way to do this?

  // (on windows, it doesn't matter what order we do it in)
  my_pclose3 (&videopipe);
  SDL_WaitThread (videopipe.outthread, &s);
  SDL_WaitThread (videopipe.errthread, &s);

  my_pclose3 (&soundpipe);
  SDL_WaitThread (soundpipe.outthread, &s);
  SDL_WaitThread (soundpipe.errthread, &s);

  // muxing and temp file cleanup

  lprintf (LO_INFO, "I_CaptureFinish: opening pipe \"%s\"\n", muxpipe.command);

  if (!my_popen3 (&muxpipe))
  {
    lprintf (LO_ERROR, "I_CaptureFinish: finalize pipe failed\n");
    return;
  }

  muxpipe.stdoutdumpname = "mux_stdout.txt";
  muxpipe.stderrdumpname = "mux_stderr.txt";
  muxpipe.outthread = SDL_CreateThread (threadstdoutproc, "muxpipe.outthread", &muxpipe);
  muxpipe.errthread = SDL_CreateThread (threadstderrproc, "muxpipe.errthread", &muxpipe);

  my_pclose3 (&muxpipe);
  SDL_WaitThread (muxpipe.outthread, &s);
  SDL_WaitThread (muxpipe.errthread, &s);


  // unlink any files user wants gone
  if (cap_remove_tempfiles)
  {
    remove (cap_tempfile1);
    remove (cap_tempfile2);
  }
}

#endif //HAVE_FFMPEG
