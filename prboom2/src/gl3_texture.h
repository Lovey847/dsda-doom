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

// Textures in texture list
enum gl3_texture_e {
  // Created from the PLAYPAL lump
  // Used as a lookup table for colors
  GL3_TEX_PLAYPAL = 0,

  // Texture pages
  GL3_TEX_PATCHES, // Page for patches

  GL3_TEX_COUNT
};

// Texture coordinates
typedef struct gl3_texcoord_s {
  short x, y;
} gl3_texcoord_t;

// OpenGL patch information
typedef struct gl3_patch_s {
  // Top left and bottom right of patch in texture page
  gl3_texcoord_t tl, br;

  // Offset to top left of patch
  int leftoffset, topoffset;

  // Patch size
  int width, height;
} gl3_patch_t;

// Texture objects
extern GLuint gl3_textures[GL3_TEX_COUNT];

// Initialize texture objects
void gl3_InitTextures(void);

// Delete textures
void gl3_DeleteTextures(void);

#endif //_GL3_TEXTURE_H
