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
typedef enum gl3_shaderid_e {
  GL3_SHADER_LINE = 0,
  GL3_SHADER_PATCH,
  GL3_SHADER_WALL,

  GL3_SHADER_COUNT
} gl3_shaderid_t;

// OpenGL shader
typedef struct gl3_shader_s {
  GLuint program; // Shader program
} gl3_shader_t;

// Uniform block used in shaders
// NOTE: You must change the block in shaders if you change it here!
typedef struct gl3_block_s {
  // Column-major matrices
  // Just pretend projmat[0][0] is column0 row0, and
  // projmat[1][0] is column1 row0
  // Keep in mind how this effects memcpy'ing these
  // arrays
  GLfloat projmat[4][4], transmat[4][4], rotmat[4][4];

  GLuint palTimesTransTables; // pal * (CR_LIMIT+1)
} gl3_block_t;

// Shaders
extern gl3_shader_t gl3_shaders[GL3_SHADER_COUNT];

// Build all shaders from shader lumps
void gl3_InitShaders(void);

// Delete all shaders
void gl3_DeleteShaders(void);

#endif //_GL3_SHADER_H
