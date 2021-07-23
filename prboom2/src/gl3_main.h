/*
 * Copyright(C) 2021 by Lian Ferrand
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * DESCRIPTION:
 *   Main OpenGL 3.3 implementation header
 */

#ifndef _GL3_MAIN_H
#define _GL3_MAIN_H

#include "gl3_opengl.h"
#include "v_video.h"
#include "lprintf.h"

static const char *gl3_strerror(int errorCode) {
  switch (errorCode) {
  case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
  case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
  case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
  case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
  case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
  case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
  case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
  }

  return "Unknown error";
}

// Wrap this around every opengl call
extern int gl3_errno;

#ifndef NDEBUG
#define GL3(...)                                          \
  __VA_ARGS__;                                            \
  if ((gl3_errno = glGetError()) != GL_NO_ERROR)          \
    lprintf(LO_INFO, "Line %d of %s: %s\n",               \
            __LINE__, __FILE__, gl3_strerror(gl3_errno))  \

#else // NDEBUG

#define GL3(...)                                \
  __VA_ARGS__;                                  \
  gl3_errno = glGetError();

#endif // NDEBUG

// OpenGL implementation information
extern int gl3_GL_MAX_TEXTURE_SIZE;
extern int gl3_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT;

// Initialize opengl 3.3 renderer
void gl3_Init(int width, int height);

// Start drawing frame
void gl3_Start(void);

// Finish drawing frame
void gl3_Finish(void);

// for v_video.c
void gl3_FillRect(int scrn, int x, int y, int width, int height, byte color);
void gl3_DrawBackground(const char *flatname, int n);
void gl3_FillFlat(int lump, int n, int x, int y, int width, int height,
                  enum patch_translation_e flags);
void gl3_FillPatch(int lump, int n, int x, int y, int width, int height,
                   enum patch_translation_e flags);
void gl3_DrawNumPatch(int x, int y, int scrn, int lump, int cm,
                      enum patch_translation_e flags);
void gl3_DrawNumPatchPrecise(float x, float y, int scrn, int lump, int cm,
                             enum patch_translation_e flags);
void gl3_PlotPixel(int scrn, int x, int y, byte color);
void gl3_PlotPixelWu(int scr, int x, int y, byte color, int weight);
void gl3_DrawLine(fline_t *fl, int color);

#endif //_GL3_MAIN_H
