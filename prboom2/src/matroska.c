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

//////////////////////////
// Local macros
#define sizeofstr(x) (sizeof(x)-1)
#define MUX_APP (PACKAGE_NAME "_v" PACKAGE_VERSION)
#define MUX_APP_SIZE sizeofstr(MUX_APP)

////////////////////////////
// Private data
static FILE *mux_file;

/////////////////////////////
// Private functions

// Get minimum amount of bytes for EBML-encoded number
static int MinBytesForEBMLNum(uint_64_t num) {
  int ret;
  uint_64_t shift;

  // ebml-encoded numbers are always unsigned so
  // we don't have to worry about signedness here
  for (shift = 49; shift && ((num>>shift)&0xff) == 0; shift -= 7);

  // Convert shift from bit shift to
  // number of bytes in ebml-encoded number
  return shift/7+1;
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
  fps = 1000000000/fps;

  // Info
  WriteElement(f, 0x1549a966,
               MUX_APP_SIZE*2 +
               MinBytesForEBMLNum(MUX_APP_SIZE)*2 +
               MinBytesForNum(fps) +
               MinBytesForEBMLNum(MinBytesForNum(fps)) +
               7);

  WriteElement(f, 0x4d80, MUX_APP_SIZE); // MuxingApp
  WriteString(f, MUX_APP);

  WriteElement(f, 0x5741, MUX_APP_SIZE); // WritingApp
  WriteString(f, MUX_APP);

  WriteElement(f, 0x2ad7b1, MinBytesForNum(fps)); // TimestampScale
  WriteMinNum(f, fps);
}

//////////////////////////////////////
// Public functions
dboolean MKV_Init(FILE *f, int width, int height, int fps) {
  (void)width; (void)height; (void)fps;

  WriteEBMLSchema(f);
  WriteElement(f, 0x18538067, 0); // Matroska segment
  WriteInfo(f, fps);

  mux_file = f;
  return true;
}

void MKV_End(void) {
  return;
}

void MKV_WriteVideoFrame(AVPacket *p) {
  fwrite(p->data, 1, p->size, mux_file);
  av_packet_unref(p);
}
