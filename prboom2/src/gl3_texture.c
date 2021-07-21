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
 *   OpenGL 3.3 texture handling
 */

#include "gl3_main.h"
#include "gl3_texture.h"

#include "lprintf.h"
#include "i_system.h"
#include "w_wad.h"

#include "dsda/palette.h"

#include <stdio.h>

GLuint gl3_paltex;
GLuint gl3_texpages[GL3_MAXPAGES];

static void gl3_InitPal(void) {
  int x, y, z;
  int width, height, depth;

  // playpal: PLAYPAL lump
  // colmap: COLORMAP lump
  // outpal: Output palette, in RGBA8 format
  //         Filled for each combination of PLAYPAL and COLORMAP
  const byte *playpal, *colmap;
  byte *outpal;

  int colmapnum;

  dsda_playpal_t *playpaldata = dsda_PlayPalData();

  // Make palette texture
  glGenTextures(1, &gl3_paltex);

  // Number of playpals
  width = W_LumpLength(W_GetNumForName(playpaldata->lump_name))/768;

  // Number of colormaps
  colmapnum = W_GetNumForName("COLORMAP");
  height = W_LumpLength(colmapnum)/256;

  // Depth (index into colormap, always 256)
  depth = 256;

  glBindTexture(GL_TEXTURE_3D, gl3_paltex);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8,
               width, height, depth,
               0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  // If we failed to create the texture, error out
  if (glGetError() != GL_NO_ERROR)
    I_Error("gl3_InitTextures: Cannot create palette texture!\n");

  // Fill palette texture, one palette at a time
  playpal = V_GetPlaypal();
  colmap = W_CacheLumpNum(colmapnum);
  outpal = (byte*)Z_Malloc(depth*4, PU_STATIC, NULL);

  for (x = 0; x < width; ++x) {
    for (y = 0; y < height; ++y) {
      for (z = 0; z < depth; ++z) {
        const size_t ind = 768*x + 3*colmap[256*y + z];

        outpal[z*4] = playpal[ind];
        outpal[z*4 + 1] = playpal[ind+1];
        outpal[z*4 + 2] = playpal[ind+2];
        outpal[z*4 + 3] = 255 * (z != playpaldata->transparent);
      }

      glTexSubImage3D(GL_TEXTURE_3D, 0,
                      x, y, z,
                      1, 1, depth,
                      GL_RGBA, GL_UNSIGNED_BYTE, outpal);
    }
  }

  Z_Free(outpal);
  W_UnlockLumpNum(colmapnum);
}

void gl3_InitTextures(void) {
  // Initialize textures
  gl3_InitPal();

  // TODO: Make texture pages!

  // TODO: When OpenGL 3.3 is fully implemented, we must _actually_ free up
  //       OpenGL resources when switching video modes, instead of doing it
  //       at exit, since VID_MODEGL and VID_MODEGL3 share resources!
  I_AtExit(gl3_DeleteTextures, true);
}

void gl3_DeleteTextures(void) {
  glDeleteTextures(1, &gl3_paltex);

  // TODO: When texture pages are implemented, delete them here!
}
