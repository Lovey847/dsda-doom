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
