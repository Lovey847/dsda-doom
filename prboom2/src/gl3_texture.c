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

GLuint gl3_textures[GL3_TEX_COUNT];

void gl3_InitTextures(void) {
  // Used when constructing outpal
  static const byte alphaTable[2] = {255, 0};

  int i, j;

  // playpal: PLAYPAL lump
  // outpal: PLAYPAL formatted into RGBA8, using transparency from playpaldata->transparency
  int playpalsize;
  const byte *playpal;
  byte *outpal;

  dsda_playpal_t *playpaldata = dsda_PlayPalData();
  
  // Initialize textures
  glGenTextures(GL3_TEX_COUNT, gl3_textures);

  for (i = 0; i < GL3_TEX_COUNT; ++i) {
    glActiveTexture(GL_TEXTURE0+i);
    glBindTexture(GL_TEXTURE_2D, gl3_textures[i]);

    // Set texture parameters
    // TODO: Make filtering an option at some point!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create texture image, this step is done differently for each texture
    switch (i) {
    case GL3_TEX_PLAYPAL:
      // Load playpal lump into texture
      playpalsize = W_LumpLength(W_GetNumForName(playpaldata->lump_name));
      playpal = V_GetPlaypal();

      // Process into RGBA8 format (add transparency byte every 3 color bytes)
      outpal = (byte*)Z_Malloc(playpalsize+playpalsize/3, PU_STATIC, NULL);

      for (j = playpalsize/3; j--;) {
        outpal[j*4] = playpal[j*3];
        outpal[j*4 + 1] = playpal[j*3 + 1];
        outpal[j*4 + 2] = playpal[j*3 + 2];
        outpal[j*4 + 3] = alphaTable[(j&255) == playpaldata->transparent];
      }

      // Set texture image
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                   256, playpalsize/768, // 768 == size of single palette in playpal
                   0, GL_RGBA, GL_UNSIGNED_BYTE, outpal);

      // Free extra palette
      Z_Free(outpal);

      break;

    default:
      // Texture page, take biggest size available
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                   gl3_GL_MAX_TEXTURE_SIZE, gl3_GL_MAX_TEXTURE_SIZE,
                   0, GL_RED, GL_UNSIGNED_BYTE, NULL);

      break;
    }
  }

  // TODO: When OpenGL 3.3 is fully implemented, we must _actually_ free up
  //       OpenGL resources when switching video modes, instead of doing it
  //       at exit, since VID_MODEGL and VID_MODEGL3 share resources!
  I_AtExit(gl3_DeleteTextures, true);
}

void gl3_DeleteTextures(void) {
  glDeleteTextures(GL3_TEX_COUNT, gl3_textures);
}
