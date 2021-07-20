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
 *   Main OpenGL 3.3 implementation
 */

#ifndef _GL3_TEXTURE_H
#define _GL3_TEXTURE_H

#include "gl3_main.h"

enum gl3_texture_e {
  // Texture atlas of patches
  GL3_TEX_PATCHES = 0,
  
  GL3_TEX_COUNT
};

// Texture objects
extern GLuint gl3_textures[GL3_TEX_COUNT];

// Initialize texture objects
void gl3_InitTextures(void);

// Delete textures
void gl3_DeleteTextures(void);

#endif //_GL3_TEXTURE_H
