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

#ifndef _GL3_TEXTURE_H
#define _GL3_TEXTURE_H

#include "gl3_main.h"

// Texture coordinates
typedef struct gl3_texcoord_s {
  short x, y;
} gl3_texcoord_t;

// OpenGL patch information
typedef struct gl3_patch_s {
  // Top left and bottom right of patch in texture page
  // NOTE: If tl.y > br.y, that means the patch was
  //       rotated 90 degrees!
  gl3_texcoord_t tl, br;

  // Offset to top left of patch
  int leftoffset, topoffset;

  // Patch size
  int width, height;
} gl3_patch_t;

// Palette texture
// 3D texture filled with every combination of
// PLAYPAL and COLORMAP for speedy palette lookup
// X = playpal choice, Y = colormap choice, Z = colormap index
extern GLuint gl3_paltex;

// Texture pages
#define GL3_MAXPAGES 8
extern GLuint gl3_texpages[GL3_MAXPAGES];

// Initialize texture objects
void gl3_InitTextures(void);

// Delete textures
void gl3_DeleteTextures(void);

#endif //_GL3_TEXTURE_H
