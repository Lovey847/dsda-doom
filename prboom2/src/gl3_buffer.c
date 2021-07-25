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

#include "gl3_buffer.h"

#include "i_system.h"
#include "v_video.h"

///////////////////////////
// OpenGL buffer handling

static GLuint CreateVAO(void) {
  GLuint ret;

  GL3(gl3_glGenVertexArrays(1, &ret));
  GL3(gl3_glBindVertexArray(ret));
  return ret;
}

static GLuint CreateBuffer(GLenum type, GLsizeiptr size) {
  GLuint ret;

  GL3(gl3_glGenBuffers(1, &ret));
  GL3(gl3_glBindBuffer(type, ret));
  GL3(gl3_glBufferData(type, size, NULL, GL_STREAM_DRAW));
  return ret;
}

static void SetupVAO(GLuint vao) {
  GL3(gl3_glVertexAttribPointer(0,
                                4, GL_FLOAT, GL_FALSE,
                                sizeof(gl3_vert_t), NULL));
  GL3(gl3_glVertexAttribPointer(1,
                                2, GL_UNSIGNED_SHORT, GL_FALSE,
                                sizeof(gl3_vert_t), (void*)offsetof(gl3_vert_t, coord)));

  GL3(gl3_glEnableVertexAttribArray(0));
  GL3(gl3_glEnableVertexAttribArray(1));
}

//////////////////////////
// Buffer handling

// Local buffer objects
static GLuint vao, vbo, ebo, ubo;

// Local buffer pointers
static size_t curvert = 0, curind = 0;

// Raw buffers
gl3_vert_t *gl3_verts;
unsigned short *gl3_inds;

// Maximum buffer sizes
size_t gl3_vertcount;
size_t gl3_indcount;

// Uniform buffer data
gl3_block_t gl3_shaderdata;

void gl3_InitBuffers(size_t verts, size_t inds) {
  vao = CreateVAO();

  vbo = CreateBuffer(GL_ARRAY_BUFFER, sizeof(gl3_vert_t)*verts);
  ebo = CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, 2*inds);

  SetupVAO(vao);

  ubo = CreateBuffer(GL_UNIFORM_BUFFER, sizeof(gl3_block_t));
  GL3(gl3_glBindBufferRange(GL_UNIFORM_BUFFER, 0, ubo, 0, sizeof(gl3_block_t)));

  gl3_vertcount = verts;
  gl3_indcount = inds;

  gl3_verts = Z_Malloc(sizeof(gl3_vert_t)*verts, PU_STATIC, NULL);
  gl3_inds = Z_Malloc(2*inds, PU_STATIC, NULL);

  I_AtExit(gl3_DeleteBuffers, true);
}

void gl3_DeleteBuffers(void) {
  GL3(gl3_glBindBuffer(GL_ARRAY_BUFFER, 0));
  GL3(gl3_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  GL3(gl3_glBindBuffer(GL_UNIFORM_BUFFER, 0));
  GL3(gl3_glBindVertexArray(0));

  GL3(gl3_glDeleteBuffers(1, &ubo));
  GL3(gl3_glDeleteBuffers(1, &ebo));
  GL3(gl3_glDeleteBuffers(1, &vbo));
  GL3(gl3_glDeleteVertexArrays(1, &vao));

  Z_Free(gl3_verts);
  Z_Free(gl3_inds);

  gl3_vertcount = 0;
  gl3_indcount = 0;
}

void gl3_DrawBuffers(void) {
  // Orphan buffers
  GL3(gl3_glBufferData(GL_ARRAY_BUFFER, sizeof(gl3_vert_t)*gl3_vertcount, NULL, GL_STREAM_DRAW));
  GL3(gl3_glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(gl3_vert_t)*curvert, gl3_verts));
  GL3(gl3_glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2*gl3_indcount, NULL, GL_STREAM_DRAW));
  GL3(gl3_glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 2*curind, gl3_inds));
  GL3(gl3_glBufferData(GL_UNIFORM_BUFFER, sizeof(gl3_block_t), NULL, GL_STREAM_DRAW));
  GL3(gl3_glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(gl3_block_t), &gl3_shaderdata));

  GL3(glDrawElements(GL_TRIANGLES, curind, GL_UNSIGNED_SHORT, NULL));

  curvert = curind = 0; // Make sure to reset buffer points
}

void gl3_AddVerts(const gl3_vert_t *verts, size_t vertcnt,
                  const unsigned short *inds, size_t indcnt)
{
  size_t i;

  if (curvert+vertcnt > gl3_vertcount) I_Error("gl3_AddVerts: Vertex buffer overflow!\n");
  if (curind+indcnt > gl3_indcount) I_Error("gl3_AddVerts: Index buffer overflow!\n");

  memcpy(gl3_verts + curvert, verts, sizeof(gl3_vert_t)*vertcnt);

  // We have to add curvert to inds as an offset
  for (i = 0; i < indcnt; ++i)
    gl3_inds[i+curind] = inds[i]+curvert;

  curvert += vertcnt;
  curind += indcnt;
}

void gl3_AddImage(const gl3_img_t *img, float x, float y, enum patch_translation_e flags) {
  static const float two_over_320 = 2.f/320.f;
  static const float negative_two_over_200 = -2.f/200.f;

  const stretch_param_t * const params = &stretch_params[flags&VPT_ALIGN_MASK];

  const float two_over_width = 2.f/(float)SCREENWIDTH;
  const float negative_two_over_height = -2.f/(float)SCREENHEIGHT;

  gl3_vert_t verts[4] = {};
  float ex, ey; // End point

  if (!(flags & VPT_NOOFFSET)) {
    x -= img->leftoffset;
    y -= img->topoffset;
  }

  // Convert to normalized coordinates
  if (flags&VPT_STRETCH_MASK) {
    const float two_over_vidwidth = 2.f/(float)params->video->width;
    const float negative_two_over_vidheight = -2.f/(float)params->video->height;
    const float vidwidth_over_width = (float)params->video->width/(float)SCREENWIDTH;
    const float vidheight_over_height = (float)params->video->height/(float)SCREENHEIGHT;

    x = x*two_over_320*vidwidth_over_width-1.f;
    y = y*negative_two_over_200*vidheight_over_height+1.f; // Flip Y coordinate
    ex = x + (float)img->width*two_over_320*vidwidth_over_width;
    ey = y + (float)img->height*negative_two_over_200*vidheight_over_height;

    // Add screen properties
    x += (float)params->deltax1*two_over_width; // As far as I know, deltax2 == deltax1
    y += (float)params->deltay1*negative_two_over_height;
    ex += (float)params->deltax1*two_over_width;
    ey += (float)params->deltay1*negative_two_over_height;
  } else {
    x = x*two_over_width-1.f;
    y = y*negative_two_over_height+1.f;
    ex = x + (float)img->width*two_over_width;
    ey = y + (float)img->height*negative_two_over_height;
  }

  // Fill out verts
  verts[0].x = x;
  verts[0].y = y;
  verts[0].coord = img->tl;

  verts[1].x = ex;
  verts[1].y = y;
  verts[1].coord = img->tr;

  verts[2].x = x;
  verts[2].y = ey;
  verts[2].coord = img->bl;

  verts[3].x = ex;
  verts[3].y = ey;
  verts[3].coord = img->br;

  // Draw quad
  gl3_AddQuad(verts);
}