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
 *   OpenGL 3.3 buffer handling
 */

#ifndef _GL3_BUFFER_H
#define _GL3_BUFFER_H

#include "gl3_main.h"
#include "gl3_texture.h"
#include "gl3_shader.h"

// Rendering vertex flags
enum {
  GL3_VERTFLAG_TRANS = 0,

  GL3_VERTFLAG_TRANSMASK = 0xf << GL3_VERTFLAG_TRANS,
};

// Rendering vertex
typedef struct gl3_vert_s {
  float x, y, z; // Normalized, 0-1

  // imgcoord: Top left coordinate of image in texture page
  // imgsize: Size of image in texture page
  // coord: Coordinate inside image in texture page (wraps around)
  gl3_texcoord_t imgcoord, imgsize, coord;
  unsigned int flags;
} gl3_vert_t;

// Raw vertex & index buffer
extern gl3_vert_t *gl3_verts;
extern unsigned short *gl3_inds;

// Maximum number of vertices and indices
extern size_t gl3_vertcount;
extern size_t gl3_indcount;

// Uniform buffer
extern gl3_block_t gl3_shaderdata;

// Initialize buffers, specifying buffer sizes
void gl3_InitBuffers(size_t verts, size_t inds);

// Delete buffers
void gl3_DeleteBuffers(void);

// Draw contents from buffers into OpenGL
void gl3_DrawBuffers(void);

// Add vertices to buffer
void gl3_AddVerts(const gl3_vert_t *verts, size_t vertcnt,
                   const unsigned short *inds, size_t indcnt);

// Add image to buffer at specified position
void gl3_AddImage(const gl3_img_t *img, float x, float y, float width, float height,
                  int cm, enum patch_translation_e flags);

// Add triangle to buffer
static const unsigned short gl3_triangleInds[3] = {0, 1, 2};
static INLINE void gl3_AddTriangle(const gl3_vert_t verts[3]) {
  gl3_AddVerts(verts, 3, gl3_triangleInds, 3);
}

// Add quad to buffer
static const unsigned short gl3_quadInds[6] = {0, 1, 2, 3, 1, 2};
static INLINE void gl3_AddQuad(const gl3_vert_t verts[4]) {
  gl3_AddVerts(verts, 4, gl3_quadInds, 6);
}

#endif //_GL3_BUFFER_H
