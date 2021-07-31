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
 *   Player view rendering
 */

#include "gl3_view.h"
#include "gl3_buffer.h"
#include "gl3_texture.h"

#include "r_main.h"

#include <math.h>

static const float invFrac = 1.f/(float)FRACUNIT;

void gl3_SetViewMatrices(mobj_t *player) {
//static const double angletorad = 2.0/4294967296.0*3.14159265358979323846;
  static const double angletorad = 0.00000000146291807927;

  // Player angle in radians
  // Kinda hard to get considering how huge the number can be
  double dir;

  // Kinda cool you can do this in one multiply
  dir = (double)player->angle*angletorad;

  // Set transformation matrices
  memset(gl3_shaderdata.projmat, 0, sizeof(gl3_shaderdata.projmat));
  gl3_shaderdata.projmat[3][1] = (float)(SCREENHEIGHT/2-viewwindowy-viewheight/2)/(float)SCREENHEIGHT;
  gl3_shaderdata.projmat[0][0] = (float)scaledviewwidth/(float)(SCREENWIDTH*SCREENWIDTH);
  gl3_shaderdata.projmat[1][1] = gl3_shaderdata.projmat[0][0] * (float)SCREENWIDTH/(float)SCREENHEIGHT;
  gl3_shaderdata.projmat[2][2] = 1.f;
  gl3_shaderdata.projmat[3][3] = 1.f;

  memset(gl3_shaderdata.transmat, 0, sizeof(gl3_shaderdata.transmat));
  gl3_shaderdata.transmat[3][0] = -(float)player->x*invFrac;
  gl3_shaderdata.transmat[3][1] = -(float)player->y*invFrac;
  gl3_shaderdata.transmat[0][0] = 1.f;
  gl3_shaderdata.transmat[1][1] = 1.f;
  gl3_shaderdata.transmat[2][2] = 1.f;
  gl3_shaderdata.transmat[3][3] = 1.f;

  memset(gl3_shaderdata.rotmat, 0, sizeof(gl3_shaderdata.rotmat));
  gl3_shaderdata.rotmat[0][0] = cos(dir);
  gl3_shaderdata.rotmat[1][0] = sin(dir);
  gl3_shaderdata.rotmat[0][1] = -sin(dir);
  gl3_shaderdata.rotmat[1][1] = cos(dir);
  gl3_shaderdata.rotmat[2][2] = 1.f;
  gl3_shaderdata.rotmat[3][3] = 1.f;
}

void gl3_DrawWall(seg_t *line, mobj_t *player) {
  // Quad vertices
  gl3_vert_t verts[4];

  // TODO: This is all a test, actually implement the shaders, matrices and math needed
  // to draw walls!
  line_t *l = line->linedef;
  side_t *s = line->sidedef;
  const gl3_img_t *img = gl3_GetWall(s->midtexture);
  float x1, y1, x2, y2, dx, dy;
  union {
    float f;
    unsigned i;
  } dir;

  if (s->midtexture == 0) return;

  x1 = (float)line->v1->x*invFrac;
  y1 = (float)line->v1->y*invFrac;
  x2 = (float)line->v2->x*invFrac;
  y2 = (float)line->v2->y*invFrac;
  dx = (float)l->dx*invFrac;
  dy = (float)l->dy*invFrac;

  verts[0].x = x1;
  verts[0].y = y1;
  verts[0].z = 0.f;
  verts[0].coord.x = 0;
  verts[0].coord.y = 0;

  verts[1].x = x2;
  verts[1].y = y2;
  verts[1].z = 0.f;
  verts[1].coord.x = sqrt(dx*dx + dy*dy);
  verts[1].coord.y = 0;

  verts[1].imgcoord = img->tl;
  verts[1].imgsize.x = img->width;
  verts[1].imgsize.y = img->height;
  verts[1].flags = 0; // No flags

  gl3_AddVerts(verts, 2, NULL, 0, GL3_BUF_WALLS);
}
