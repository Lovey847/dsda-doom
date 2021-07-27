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

#include "r_state.h"

// Textures used in renderer
enum {
  // 3D RGBA8 palette texture containing every
  // combination of PLAYPAL and COLORMAP for speedy
  // palette lookup
  // X = playpal choice, Y = colormap choice, Z = colormap index
  GL3_TEXTURE_PALETTE = 0,

  // 2D R8UI texture page containing every patch, sprite, etc.
  GL3_TEXTURE_PAGE,

  GL3_TEXTURE_COUNT
};

// Texture coordinates
typedef struct gl3_texcoord_s {
  short x, y;
} gl3_texcoord_t;

// Image from texture page
typedef struct gl3_img_s {
  // Corners of texture in texture page
  gl3_texcoord_t tl, br;

  // Offset to top left of patch
  // 0, 0 if not applicable
  int leftoffset, topoffset;

  // Width and height of patch
  int width, height;
} gl3_img_t;

// List of images in texture page
extern gl3_img_t *gl3_images;
extern size_t gl3_imagecount;

extern GLuint gl3_textures[GL3_TEXTURE_COUNT];

// Initialize texture objects
void gl3_InitTextures(void);

// Delete textures
void gl3_DeleteTextures(void);

// Get patch from lump number
// Returns NULL if patch is not in texture
const gl3_img_t *gl3_GetPatch(int lump);

// Get wall texture from texture ID
const gl3_img_t *gl3_GetWall(int id);

// Get flat from lump number
// Returns NULL if flat is not in texture
static INLINE const gl3_img_t *gl3_GetFlat(int lump) {
  return gl3_GetPatch(lump+firstflat);
}

#endif //_GL3_TEXTURE_H
