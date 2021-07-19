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
 *   OpenGL 3.3 function loader
 */

#include "gl3_main.h"
#include "r_defs.h"
#include "gl_struct.h"

// TODO: Right now, these are wrappers for gld functions.
//       Implement these at some point!
void gl3_FillRect(int scrn, int x, int y, int width, int height, byte color) {
  gld_FillBlock(x, y, width, height, color);
}

void gl3_DrawBackground(const char *flatname, int n) {
  gld_FillFlatName(flatname, 0, 0, SCREENWIDTH, SCREENHEIGHT, VPT_NONE);
}

void gl3_FillFlat(int lump, int n, int x, int y, int width, int height,
                  enum patch_translation_e flags)
{
  gld_FillFlat(lump, x, y, width, height, flags);
}

void gl3_FillPatch(int lump, int n, int x, int y, int width, int height,
                   enum patch_translation_e flags)
{
  gld_FillPatch(lump, x, y, width, height, flags);
}

void gl3_DrawNumPatch(int x, int y, int scrn, int lump, int cm,
                      enum patch_translation_e flags)
{
  gld_DrawNumPatch(x, y, lump, cm, flags);
}

void gl3_DrawNumPatchPrecise(float x, float y, int scrn, int lump, int cm,
                             enum patch_translation_e flags)
{
  gld_DrawNumPatch_f(x, y, lump, cm, flags);
}

void gl3_PlotPixel(int scrn, int x, int y, byte color) {
  gld_DrawLine(x-1, y, x+1, y, color);
  gld_DrawLine(x, y-1, x, y+1, color);
}

void gl3_PlotPixelWu(int scr, int x, int y, byte color, int weight) {
  gld_DrawLine(x-1, y, x+1, y, color);
  gld_DrawLine(x, y-1, x, y+1, color);
}

void gl3_DrawLine(fline_t *fl, int color) {
  gld_DrawLine_f(fl->a.fx, fl->a.fy, fl->b.fx, fl->b.fy, color);
}
