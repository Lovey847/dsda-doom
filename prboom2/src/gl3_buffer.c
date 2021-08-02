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
#include "gl3_view.h"

#include "i_system.h"
#include "v_video.h"
#include "doomstat.h"
#include "r_main.h"

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
  GL3(gl3_glVertexAttribIPointer(1,
                                 2, GL_SHORT,
                                 sizeof(gl3_vert_t), (void*)offsetof(gl3_vert_t, imgcoord)));
  GL3(gl3_glVertexAttribIPointer(2,
                                 2, GL_SHORT,
                                 sizeof(gl3_vert_t), (void*)offsetof(gl3_vert_t, imgsize)));
  GL3(gl3_glVertexAttribPointer(3,
                                2, GL_SHORT, GL_FALSE,
                                sizeof(gl3_vert_t), (void*)offsetof(gl3_vert_t, coord)));
  GL3(gl3_glVertexAttribIPointer(4,
                                 1, GL_UNSIGNED_INT,
                                 sizeof(gl3_vert_t), (void*)offsetof(gl3_vert_t, flags)));

  GL3(gl3_glEnableVertexAttribArray(0));
  GL3(gl3_glEnableVertexAttribArray(1));
  GL3(gl3_glEnableVertexAttribArray(2));
  GL3(gl3_glEnableVertexAttribArray(3));
  GL3(gl3_glEnableVertexAttribArray(4));
}

static void OrphanBuffer(GLenum buf, GLsizeiptr bufsize, GLsizeiptr size, void *data) {
  GL3(gl3_glBufferData(buf, bufsize, NULL, GL_STREAM_DRAW));
  GL3(gl3_glBufferSubData(buf, 0, size, data));
}

//////////////////////////
// Buffer handling

// Local buffer objects
static GLuint vao, vbo, ebo, ubo;

// Local buffer pointers
static size_t curvert = 0, curind = 0;

// Raw buffers
static gl3_vert_t *gl3_verts;
static unsigned short *gl3_inds;

// Maximum buffer sizes
static size_t gl3_vertcount;
static size_t gl3_indcount;

// Current active buffer
static gl3_buffer_t curbuf = GL3_BUF_NONE;

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

// Flush current buffer
void gl3_FlushBuffers(void) {
  // No vertices to draw
  if (!curvert) return;

  // Setup uniforms before orphaning
  switch (curbuf) {
  case GL3_BUF_LINES:
    lprintf(LO_DEBUG, "gl3_FlushBuffers: Drawing line batch\n");

    // Orphan buffers
    OrphanBuffer(GL_UNIFORM_BUFFER, sizeof(gl3_block_t), sizeof(gl3_block_t), &gl3_shaderdata);
    OrphanBuffer(GL_ARRAY_BUFFER, sizeof(gl3_vert_t)*gl3_vertcount,
                 sizeof(gl3_vert_t)*curvert, gl3_verts);

    GL3(gl3_glUseProgram(gl3_shaders[GL3_SHADER_LINE].program));
    GL3(glDrawArrays(GL_LINES, 0, curvert));
    break;
  case GL3_BUF_PATCHES:
    lprintf(LO_DEBUG, "gl3_FlushBuffers: Drawing patch batch\n");

    // Orphan buffers
    OrphanBuffer(GL_UNIFORM_BUFFER, sizeof(gl3_block_t), sizeof(gl3_block_t), &gl3_shaderdata);
    OrphanBuffer(GL_ARRAY_BUFFER, sizeof(gl3_vert_t)*gl3_vertcount,
                 sizeof(gl3_vert_t)*curvert, gl3_verts);
    OrphanBuffer(GL_ELEMENT_ARRAY_BUFFER, 2*gl3_indcount,
                 2*curind, gl3_inds);

    GL3(gl3_glUseProgram(gl3_shaders[GL3_SHADER_PATCH].program));
    GL3(glDrawElements(GL_TRIANGLES, curind, GL_UNSIGNED_SHORT, NULL));
    break;
  case GL3_BUF_WALLS:
    lprintf(LO_DEBUG, "gl3_FlushBuffers: Drawing wall batch\n");

    // Set uniforms
    gl3_SetViewMatrices(players[displayplayer].mo);

    // Orphan buffers
    OrphanBuffer(GL_UNIFORM_BUFFER, sizeof(gl3_block_t), sizeof(gl3_block_t), &gl3_shaderdata);
    OrphanBuffer(GL_ARRAY_BUFFER, sizeof(gl3_vert_t)*gl3_vertcount,
                 sizeof(gl3_vert_t)*curvert, gl3_verts);
    OrphanBuffer(GL_ELEMENT_ARRAY_BUFFER, 2*gl3_indcount,
                 2*curind, gl3_inds);

    // Enable depth buffer
    GL3(glDepthFunc(GL_LESS));

    // Set viewport
    GL3(glViewport(viewwindowx, SCREENHEIGHT-viewheight-viewwindowy, scaledviewwidth, viewheight));

    GL3(gl3_glUseProgram(gl3_shaders[GL3_SHADER_WALL].program));
    GL3(glDrawElements(GL_TRIANGLES, curind, GL_UNSIGNED_SHORT, NULL));

    GL3(glDepthFunc(GL_ALWAYS));
    GL3(glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT));
    break;

  default: lprintf(LO_WARN, "gl3_FlushBuffers: Unknown buffer active!? (%d)\n", curbuf); return;
  }

  // Reset buffer
  curvert = curind = 0;
  curbuf = GL3_BUF_NONE;
}

void gl3_AddVerts(const gl3_vert_t *verts, size_t vertcnt,
                  const unsigned short *inds, size_t indcnt,
                  gl3_buffer_t buf)
{
  size_t i;

  if (buf != curbuf) {
    gl3_FlushBuffers();
    curbuf = buf;
  }

  if (curvert+vertcnt > gl3_vertcount) {
    const size_t oldverts = gl3_vertcount;

    // Resize vertex buffer
    while (curvert+vertcnt > gl3_vertcount) gl3_vertcount *= 2;
    gl3_verts = Z_Realloc(gl3_verts, sizeof(gl3_vert_t)*gl3_vertcount, PU_STATIC, NULL);

    lprintf(LO_WARN, "gl3_AddVerts: Resized vertex buffer from %u to %u\n",
            (unsigned)oldverts, (unsigned)gl3_vertcount);
  }
  if (curind+indcnt > gl3_indcount) {
    const size_t oldinds = gl3_indcount;

    // Resize index buffer
    while (curind+indcnt > gl3_indcount) gl3_indcount *= 2;
    gl3_inds = Z_Realloc(gl3_inds, 2*gl3_indcount, PU_STATIC, NULL);

    lprintf(LO_WARN, "gl3_AddVerts: Resized index buffer from %u to %u\n",
            (unsigned)oldinds, (unsigned)gl3_indcount);
  }

  memcpy(gl3_verts + curvert, verts, sizeof(gl3_vert_t)*vertcnt);

  // We have to add curvert to inds as an offset
  for (i = 0; i < indcnt; ++i)
    gl3_inds[i+curind] = inds[i]+curvert;

  curvert += vertcnt;
  curind += indcnt;
}

void gl3_AddImage(const gl3_img_t *img, float x, float y, float width, float height,
                  int cm, enum patch_translation_e flags)
{
  // TODO: I have transformation matrices in the shader now,
  // I can heavily optimize this operation!
  static const float one_over_320 = 1.f/320.f;
  static const float one_over_200 = 1.f/200.f;

  const stretch_param_t * const params = &stretch_params[flags&VPT_ALIGN_MASK];

  const float two_over_width = 2.f/(float)SCREENWIDTH;
  const float negative_two_over_height = -2.f/(float)SCREENHEIGHT;

  gl3_vert_t verts[4] = {
    {}, {},
    {0.f, 0.f, 0.f, {0, 0}, {999, 999}}
  };
  float ex, ey; // End point
  GLuint vflags = 0;

  if (!(flags & VPT_NOOFFSET)) {
    x -= img->leftoffset;
    y -= img->topoffset;
  }

  if ((flags&VPT_TRANS) && (cm < CR_LIMIT)) vflags |= ((cm+1)<<GL3_PFLAG_TRANS)&GL3_PFLAG_TRANSMASK;

  // Convert to normalized coordinates
  if (flags&VPT_STRETCH_MASK) {
    // x2lookup[n] == x1lookup[n+1]-1, which is good for the software renderer,
    // but not very good for opengl
    const size_t xi = (size_t)x;
    const size_t yi = (size_t)y;
    const size_t exi = (size_t)(x+width);
    const size_t eyi = (size_t)(y+height);

    // NOTE: If exi or eyi is less than 0, or xi
    // is greater than 320, or yi is greater than 200,
    // we can skip drawing this image
    // Note because this code is slow and I'm gonna improve it in the future
    if ((exi < 0) || (exi > 320))
      ex = (x+width)*(float)params->video->width*one_over_320;
    else
      ex = (float)params->video->x1lookup[exi];

    if ((eyi < 0) || (eyi > 200))
      ey = (y+height)*(float)params->video->height*one_over_200;
    else
      ey = (float)params->video->y1lookup[eyi];

    if ((xi < 0) || (xi > 320))
      x = x*(float)params->video->width*one_over_320;
    else
      x = (float)params->video->x1lookup[xi];

    if ((yi < 0) || (yi > 320))
      y = y*(float)params->video->height*one_over_200;
    else
      y = (float)params->video->y1lookup[yi];

    // Add screen properties
    x += (float)params->deltax1;
    y += (float)params->deltay1;
    ex += (float)params->deltax2;
    ey += (float)params->deltay1;
  } else {
    ex = x+width;
    ey = y+height;
  }

  x = x*two_over_width-1.f;
  y = y*negative_two_over_height+1.f;
  ex = ex*two_over_width-1.f;
  ey = ey*negative_two_over_height+1.f;

  if (flags&VPT_FLIP) {
    const float tmp = ex;
    ex = x;
    x = tmp;
  }

  // Fill out verts
  verts[0].x = x;
  verts[0].y = y;
  verts[0].coord.x = 0;
  verts[0].coord.y = 0;

  verts[1].x = ex;
  verts[1].y = y;
  verts[1].coord.x = width;
  verts[1].coord.y = 0;

  verts[2].x = x;
  verts[2].y = ey;
  verts[2].coord.x = 0;
  verts[2].coord.y = height;

  verts[3].x = ex;
  verts[3].y = ey;
  verts[3].coord.x = width;
  verts[3].coord.y = height;

  // Set flat attributes for provoking vertex
  verts[2].imgcoord = img->tl;

  // If width == img->width and height == img->height, artifacting
  // occurs in flipped images, make sure that doesn't happen!
  // (imgsize set to {999, 999} above
  if ((width != img->width) || (height != img->height)) {
    verts[2].imgsize.x = img->width;
    verts[2].imgsize.y = img->height;
  }
  verts[2].flags = vflags;

  // Draw quad
  gl3_AddQuad(verts, GL3_BUF_PATCHES);
}
