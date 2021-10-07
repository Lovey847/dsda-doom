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

#include "matroska.h"
#include "doomstat.h"

//////////////////////////
// Local macros
#define sizeofstr(x) (sizeof(x)-1)
#define MUX_APP (PACKAGE_NAME "_v" PACKAGE_VERSION)
#define MUX_APP_SIZE sizeofstr(MUX_APP)

#define V_CODEC_ID "V_MPEG4/ISO/AVC"
#define V_CODEC_ID_SIZE sizeofstr(V_CODEC_ID)

#define SEG_START 52

#define TRACK_VIDEO 1

#define BASE_CUES 128

////////////////////////////
// Private data
static FILE *mux_file;

// Position of Info's Duration element's data
// in mux_file
static size_t durationPos;

// Cue list entry
typedef struct cue_s {
  uint_64_t timestamp; // Absolute timestamp
  size_t cluster; // Cluster's segment position
} cue_t;

// List of cues to write after ending cluster transmittion
static cue_t *cues, *curcue; // Dynamic buffer of cues
static size_t maxcues; // Maximum number of cues, extended if necessary

/////////////////////////////
// Private functions

// Get minimum amount of bytes for EBML-encoded number
static int MinBytesForEBMLNum(uint_64_t num) {
  int ret;
  uint_64_t shift;

  // Special case for 0
  if (!num) return 1;

  // ebml-encoded numbers are always unsigned so
  // we don't have to worry about signedness here
  for (shift = 49; shift && ((num>>shift)&0xff) == 0; shift -= 7);

  // Convert shift from bit shift to
  // number of bytes in ebml-encoded number
  return shift/7+1;
}

// Write double-precision floating-point nnumber
static void WriteDouble(FILE *f, double num) {
  byte stream[8];
  byte tmp[4];
  *(double*)stream = num;

#ifndef WORDS_BIGENDIAN
  *(int*)tmp = *(int*)stream;
  stream[0] = stream[7];
  stream[1] = stream[6];
  stream[2] = stream[5];
  stream[3] = stream[4];
  stream[4] = tmp[3];
  stream[5] = tmp[2];
  stream[6] = tmp[1];
  stream[7] = tmp[0];
#endif

  fwrite(stream, 8, 1, f);
}

// Write EBML-encoded number
static void WriteEBMLNum(FILE *f, uint_64_t num) {
  byte stream[8], *s = stream+1;
  uint_64_t mask, shift, desc;

  // Special case for 0
  if (!num) {
    fputc(0x80, f);
    return;
  }

  // Make sure num is in range
  num &= LONGLONG(0xffffffffffffff);

  // Get length descriptor and shift up to the last bits of num
  for (mask = ~LONGLONG(127), desc = 128, shift = 0;
       num&mask; mask <<= 7, desc >>= 1, shift += 8);

  // Output first stream byte, based on desc and shift
  stream[0] = desc | (num>>shift);

  // Output further stream bytes
  switch (desc) {
  case   1: *s++ = (num&(LONGLONG(0xff)<<LONGLONG(48)))>>LONGLONG(48); // fallthrough
  case   2: *s++ = (num&(LONGLONG(0xff)<<LONGLONG(40)))>>LONGLONG(40); // fallthrough
  case   4: *s++ = (num&(LONGLONG(0xff)<<LONGLONG(32)))>>LONGLONG(32); // fallthrough
  case   8: *s++ = (num&(LONGLONG(0xff)<<LONGLONG(24)))>>LONGLONG(24); // fallthrough
  case  16: *s++ = (num&(LONGLONG(0xff)<<LONGLONG(16)))>>LONGLONG(16); // fallthrough
  case  32: *s++ = (num&(LONGLONG(0xff)<<LONGLONG( 8)))>>LONGLONG( 8); // fallthrough
  case  64: *s++ = num&LONGLONG(0xff);                                 // fallthrough
  case 128: ;
  }

  // Write stream to file
  fwrite(stream, 1, s-stream, f);
}

// Write NULL-terminated string
static void WriteString(FILE *f, const char *s) {
  fwrite(s, 1, strlen(s), f);
}

// Write number
static void WriteNum(FILE *f, uint_64_t num, int size) {
  byte stream[8], *s = stream;
  uint_64_t shift;

  for (shift = size*8 - 8; size--; shift -= 8) {
    *s++ = (num>>shift)&0xff;
  }

  fwrite(stream, 1, s-stream, f);
}

// Get minimum bytes required to store number
static int MinBytesForNum(uint_64_t num) {
  byte firstval; // For negative numbers
  uint_64_t shift;

  // Special case for 0
  if (!num) return 1;

  firstval = (num>>LONGLONG(56))&0xff;

  // If this holds an actual value, and isn't just padding,
  // then it's a full 64-bit number
  if ((firstval != 0) && (firstval != 0xff))
    return 8;

  // Find the most significant byte
  for (shift = 48; ((num>>shift)&0xff) == firstval; shift -= 8);

  // Translate shift from bit shift to number of bytes
  return shift/8+1;
}

// Write number in minimum amount of bytes
static void WriteMinNum(FILE *f, uint_64_t num) {
  WriteNum(f, num, MinBytesForNum(num));
}

// Write EBML element header
static void WriteElement(FILE *f, unsigned int id, uint_64_t len) {
  static const byte zerolen[8] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  // Write ID
  // Already contains length descriptor
  WriteMinNum(f, id);

  // If len is 0, that means the length of
  // this space is unknown for now
  // and will be filled in later
  if (!len) {
    fwrite(zerolen, 1, 8, f);
    return;
  }

  WriteEBMLNum(f, len);
}

// Get size of EBML element, in bytes
static size_t ElementSize(unsigned int id, uint_64_t len) {
  return MinBytesForNum(id) + MinBytesForEBMLNum(len) + len;
}

// Write EBML schema
static void WriteEBMLSchema(FILE *f) {
  WriteElement(f, 0x1a45dfa3, 35); // EBML
  WriteElement(f, 0x4286, 1); // EBMLVersion
  WriteNum(f, 1, 1);
  WriteElement(f, 0x42f7, 1); // EBMLReadVersion
  WriteNum(f, 1, 1);
  WriteElement(f, 0x42f2, 1); // EBMLMaxIDLen
  WriteNum(f, 4, 1);
  WriteElement(f, 0x42f3, 1); // EBMLMaxSizeLen
  WriteNum(f, 8, 1);
  WriteElement(f, 0x4282, 8); // DocType
  WriteString(f, "matroska");
  WriteElement(f, 0x4287, 1); // DocTypeVersion
  WriteNum(f, 3, 1);
  WriteElement(f, 0x4285, 1); // DocTypeReadVersion
  WriteNum(f, 3, 1);
}

// Write matroska info element
static void WriteInfo(FILE *f, int fps) {
  int len;

  fps = 1000000000/fps;

  len =
    ElementSize(0x4d80, MUX_APP_SIZE) + // MuxingApp
    ElementSize(0x5741, MUX_APP_SIZE) + // WritingApp
    ElementSize(0x2ad7b1, MinBytesForNum(fps)) + // TimestampScale
    ElementSize(0x4489, 8); // Duration

  WriteElement(f, 0x1549a966, len); // Info

  WriteElement(f, 0x4d80, MUX_APP_SIZE); // MuxingApp
  WriteString(f, MUX_APP);

  WriteElement(f, 0x5741, MUX_APP_SIZE); // WritingApp
  WriteString(f, MUX_APP);

  WriteElement(f, 0x2ad7b1, MinBytesForNum(fps)); // TimestampScale
  WriteMinNum(f, fps);

  WriteElement(f, 0x4489, 8); // Duration

  durationPos = ftell(f);
  WriteNum(f, 0, 8);
}

// Write matroska tracks element
static void WriteTracks(FILE *f, int width, int height, int fps) {
  int len, entryLen, videoLen;

  fps = 1000000000/fps;

  videoLen =
    ElementSize(0x9a, 1) + // FlagInterlaced
    ElementSize(0xb0, MinBytesForNum(width)) + // PixelWidth
    ElementSize(0xba, MinBytesForNum(height)); // PixelHeight

  entryLen =
    ElementSize(0xd7, 1) + // TrackNumber
    ElementSize(0x73c5, 1) + // TrackUID
    ElementSize(0x83, 1) + // TrackType
    ElementSize(0xb9, 1) + // FlagEnabled
    ElementSize(0x88, 1) + // FlagDefault
    ElementSize(0x55aa, 1) + // FlagForced
    ElementSize(0x9c, 1) + // FlagLacing
    ElementSize(0x6de7, 1) + // MinCache
    ElementSize(0x23e383, MinBytesForNum(fps)) + // DefaultDuration
    ElementSize(0x23314f, 8) + // TrackTimestampScale
    ElementSize(0x55ee, 1) + // MaxBlockAdditionID
    ElementSize(0x86, V_CODEC_ID_SIZE) + // CodecID
    ElementSize(0xaa, 1) + // CodecDecodeAll
    ElementSize(0xe0, videoLen); // Video

  len =
    ElementSize(0xae, entryLen); // TrackEntry

  WriteElement(f, 0x1654ae6b, len); // Tracks

  WriteElement(f, 0xae, entryLen); // TrackEntry

  WriteElement(f, 0xd7, 1); // TrackNumber
  WriteNum(f, TRACK_VIDEO, 1);

  WriteElement(f, 0x73c5, 1); // TrackUID
  WriteNum(f, TRACK_VIDEO, 1);

  WriteElement(f, 0x83, 1); // TrackType
  WriteNum(f, 1, 1); // (1 means video track)

  WriteElement(f, 0xb9, 1); // FlagEnabled
  WriteNum(f, 1, 1);

  WriteElement(f, 0x88, 1); // FlagDefault
  WriteNum(f, 1, 1);

  WriteElement(f, 0x55aa, 1); // FlagForced
  WriteNum(f, 0, 1);

  WriteElement(f, 0x9c, 1); // FlagLacing
  WriteNum(f, 0, 1);

  WriteElement(f, 0x6de7, 1); // MinCache
  WriteNum(f, 0, 1);

  WriteElement(f, 0x23e383, MinBytesForNum(fps)); // DefaultDuration
  WriteMinNum(f, fps);

  WriteElement(f, 0x23314f, 8); // TrackTimestampScale
  WriteDouble(f, 1.0);

  WriteElement(f, 0x55ee, 1); // MaxBlockAdditionID
  WriteNum(f, 0, 1);

  WriteElement(f, 0x86, V_CODEC_ID_SIZE); // CodecID
  WriteString(f, V_CODEC_ID);

  WriteElement(f, 0xaa, 1); // CodecDecodeAll
  WriteNum(f, 1, 1);

  WriteElement(f, 0xe0, videoLen); // Video

  WriteElement(f, 0x9a, 1); // FlagInterlaced
  WriteNum(f, 2, 1); // (2 means progressive)

  WriteElement(f, 0xb0, MinBytesForNum(width)); // PixelWidth
  WriteMinNum(f, width);

  WriteElement(f, 0xba, MinBytesForNum(height)); // PixelHeight
  WriteMinNum(f, height);
}

// Write a matroska cluster element
static void WriteCluster(FILE *f, AVPacket *p) {
  int len;
  const size_t pos = ftell(f);

  len =
    ElementSize(0xe7, MinBytesForNum(p->pts)) + // Timestamp
    ElementSize(0xa7, MinBytesForNum(pos-SEG_START)) + // Position
    ElementSize(0xa3, 4 + p->size); // SimpleBlock

  WriteElement(f, 0x1f43b675, len); // Cluster

  WriteElement(f, 0xe7, MinBytesForNum(p->pts)); // Timestamp
  WriteMinNum(f, p->pts);

  WriteElement(f, 0xa7, MinBytesForNum(pos-SEG_START)); // Position
  WriteMinNum(f, pos-SEG_START);

  WriteElement(f, 0xa3, 4 + p->size); // SimpleBlock

  // 0x80 in the last byte means that this block
  // only contains keyframes
  if (p->flags&AV_PKT_FLAG_KEY) WriteNum(f, 0x81000080, 4);
  else WriteNum(f, 0x81000000, 4);

  fwrite(p->data, 1, p->size, f);
}

// Add cue to cues
static void AddCue(size_t cluster, uint_64_t timestamp) {
  size_t cuenum = curcue-cues;

  if (cuenum >= maxcues) {
    maxcues += BASE_CUES;
    cues = Z_Realloc(cues, maxcues*sizeof(cue_t), PU_STATIC, NULL);
    curcue = cues+cuenum;
  }

  curcue->cluster = cluster;
  curcue->timestamp = timestamp;

  ++curcue;
}

// Get the size a cue_t would be if it was a CuePoint element
static int CuePointSize(const cue_t *c) {
  int len, positionsLen;

  positionsLen =
    ElementSize(0xf7, 1) + // CueTrack
    ElementSize(0xf1, MinBytesForNum(c->cluster)); // CueClusterPosition

  len =
    ElementSize(0xb3, MinBytesForNum(c->timestamp)) + // CueTime
    ElementSize(0xb7, positionsLen); // CueTrackPositions

  return ElementSize(0xbb, len);
}

// Write a matroska CuePoint element inside a Cues element
static void WriteCuePoint(FILE *f, const cue_t *c) {
  int len, positionsLen;

  positionsLen =
    ElementSize(0xf7, 1) + // CueTrack
    ElementSize(0xf1, MinBytesForNum(c->cluster)); // CueClusterPosition

  len =
    ElementSize(0xb3, MinBytesForNum(c->timestamp)) + // CueTime
    ElementSize(0xb7, positionsLen); // CueTrackPositions

  WriteElement(f, 0xbb, len); // CuePoint

  WriteElement(f, 0xb3, MinBytesForNum(c->timestamp)); // CueTime
  WriteMinNum(f, c->timestamp);

  WriteElement(f, 0xb7, positionsLen); // CueTrackPositions

  WriteElement(f, 0xf7, 1); // CueTrack
  WriteNum(f, TRACK_VIDEO, 1);

  WriteElement(f, 0xf1, MinBytesForNum(c->cluster)); // CueClusterPosition
  WriteMinNum(f, c->cluster);
}

// Write matroska cues element
static void WriteCues(FILE *f) {
  int len = 0;
  const cue_t *c;

  // Go through all cues, accumulating element size
  for (c = cues; c != curcue; ++c)
    len += CuePointSize(c);

  WriteElement(f, 0x1c53bb6b, len); // Cues

  // Write all cue point elements
  for (c = cues; c != curcue; ++c)
    WriteCuePoint(f, c);
}

//////////////////////////////////////
// Public functions
dboolean MKV_Init(FILE *f, int width, int height, int fps) {
  WriteEBMLSchema(f);
  WriteElement(f, 0x18538067, 0); // Matroska segment
  WriteInfo(f, fps);
  WriteTracks(f, width, height, fps);

  // Allocate cues
  maxcues = BASE_CUES;
  curcue = cues = Z_Malloc(maxcues*sizeof(cue_t), PU_STATIC, NULL);

  mux_file = f;

  return true;
}

void MKV_End(void) {
  size_t segmentLen;

  WriteCues(mux_file);

  // Write file size (minus EBML schema and
  // size of matroska segment element) to
  // the matroska's segment element's size
  segmentLen = ftell(mux_file)-SEG_START;

  fseek(mux_file, SEG_START-7, SEEK_SET);
  WriteNum(mux_file, segmentLen, 7);

  // Write demo length to the info element's duration element
  fseek(mux_file, durationPos, SEEK_SET);
  WriteDouble(mux_file, gametic);

  fseek(mux_file, 0, SEEK_END); // Go back to the end of the file

  // Free cues
  Z_Free(cues);
  curcue = NULL;
  maxcues = 0;
}

void MKV_WriteVideoFrame(AVPacket *p) {
  size_t cluster = ftell(mux_file)-SEG_START;

  WriteCluster(mux_file, p);

  // If this packet is a keyframe, add a cue for it
  if (p->flags&AV_PKT_FLAG_KEY) {
    AddCue(cluster, p->pts);
  }

  av_packet_unref(p);
}
