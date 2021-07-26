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
 *   OpenGL 3.3 shader handling
 */

#ifndef _GL3_SHADER_H
#define _GL3_SHADER_H

#include "gl3_main.h"

// Shader types
enum {
  GL3_SHADER_PATCH = 0,

  GL3_SHADER_COUNT
};

// OpenGL shader
typedef struct gl3_shader_s {
  GLuint program; // Shader program
} gl3_shader_t;

// Uniform block used in shaders
typedef struct gl3_block_s {
  unsigned int palTimesTransTables; // pal * (CR_LIMIT+1)
} gl3_block_t;

// Shaders
extern gl3_shader_t gl3_shaders[GL3_SHADER_COUNT];

// Build all shaders from shader lumps
void gl3_InitShaders(void);

// Delete all shaders
void gl3_DeleteShaders(void);

#endif //_GL3_SHADER_H
