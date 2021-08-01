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

#include "e6y.h"
#include "r_main.h"
#include "st_stuff.h"

#include <math.h>

static const float invFrac = 1.f/(float)FRACUNIT;

void gl3_SetViewMatrices(mobj_t *player) {
//static const double angletorad = 2.0/4294967296.0*M_PI;
  static const double angletorad = 0.00000000146291807927;

  static const float nearclip = 9.f;
  static const float farclip = 2000.f;

  static const float clipdist = farclip-nearclip;

  static const GLfloat identmat[4][4] = {
    {1.f, 0.f, 0.f, 0.f},
    {0.f, 1.f, 0.f, 0.f},
    {0.f, 0.f, 1.f, 0.f},
    {0.f, 0.f, 0.f, 1.f}
  };

  // Player angle in radians
  // Kinda hard to get considering how huge the number can be
  double dir;
  float projdist; // Distance to projection plane
  float fovy; // fov in Y axis

  fovy = render_fovy;
  projdist = tanf((float)M_PI/2.f - fovy*((float)M_PI/360.f));

  // Kinda cool you can do this in one multiply
  dir = (double)viewangle*angletorad;
  dir -= M_PI/2.0;

  // Set transformation matrices

  // Translation matrix (fixed point is converted to floating point):
  //   1, 0, 0, -viewx
  //   0, 1, 0, -viewz
  //   0, 0, 1, -viewy
  //   0, 0, 0, 1
  memcpy(gl3_shaderdata.transmat, identmat, sizeof(identmat));
  gl3_shaderdata.transmat[3][0] = -(float)viewx*invFrac;
  gl3_shaderdata.transmat[3][1] = -(float)viewz*invFrac;
  gl3_shaderdata.transmat[3][2] = -(float)viewy*invFrac;

  // Rotation matrix (angles are converted to radians):
  //   cos(viewangle),  0, sin(viewangle), 0
  //   0,               1, 0,              0
  //   -sin(viewangle), 0, cos(viewangle), 0
  //   0,               0, 0,              1
  memcpy(gl3_shaderdata.rotmat, identmat, sizeof(identmat));
  gl3_shaderdata.rotmat[0][0] = cos(dir);
  gl3_shaderdata.rotmat[2][0] = sin(dir);
  gl3_shaderdata.rotmat[0][2] = -sin(dir);
  gl3_shaderdata.rotmat[2][2] = cos(dir);

  // Projection matrix
  // TODO: Learn a better depth strategy, this (should) correctly map from
  // the near clipping plane to the far clipping plane, however the propertions
  // are really bad, further objects get so little variation in depth that if you
  // were playing a big map, it's not too unlikely shenanigans would occur.
  memcpy(gl3_shaderdata.projmat, identmat, sizeof(gl3_shaderdata.projmat));
  gl3_shaderdata.projmat[0][0] = projdist/render_ratio;
  gl3_shaderdata.projmat[1][1] = projdist;
  gl3_shaderdata.projmat[2][2] = (farclip+nearclip*2.f)/farclip;
  gl3_shaderdata.projmat[3][2] = -nearclip*2.f;
  gl3_shaderdata.projmat[2][3] = 1.f;
  gl3_shaderdata.projmat[3][3] = 0.f;
}

void gl3_DrawWall(seg_t *line, mobj_t *player) {
  // Quad vertices
  gl3_vert_t verts[4];

  line_t *l = line->linedef;
  side_t *s = line->sidedef;
  const gl3_img_t *img;
  float dx, dy, dist;
  angle_t ang1, ang2;

  if (s->midtexture == 0) return;

  ang1 = R_PointToAngleEx2(viewx, viewy, line->v1->x, line->v1->y);
  ang2 = R_PointToAngleEx2(viewx, viewy, line->v2->x, line->v2->y);
  if (ang1-ang2 > ANG180)
    return;

  img = gl3_GetWall(s->midtexture);

  dx = (float)(line->v1->x-line->v2->x)*invFrac;
  dy = (float)(line->v1->y-line->v2->y)*invFrac;
  dist = sqrtf(dx*dx + dy*dy);

  verts[0].x = (float)line->v1->x*invFrac;
  verts[0].y = (float)line->frontsector->ceilingheight*invFrac;
  verts[0].z = (float)line->v1->y*invFrac;
  verts[0].coord.x = line->offset*invFrac;
  verts[0].coord.y = 0;

  verts[1].x = (float)line->v2->x*invFrac;
  verts[1].y = (float)line->frontsector->ceilingheight*invFrac;
  verts[1].z = (float)line->v2->y*invFrac;
  verts[1].coord.x = line->offset*invFrac + dist;
  verts[1].coord.y = 0;

  verts[2].x = (float)line->v1->x*invFrac;
  verts[2].y = (float)line->frontsector->floorheight*invFrac;
  verts[2].z = (float)line->v1->y*invFrac;
  verts[2].coord.x = line->offset*invFrac;
  verts[2].coord.y = (float)(line->frontsector->ceilingheight-line->frontsector->floorheight)*invFrac;

  verts[3].x = (float)line->v2->x*invFrac;
  verts[3].y = (float)line->frontsector->floorheight*invFrac;
  verts[3].z = (float)line->v2->y*invFrac;
  verts[3].coord.x = line->offset*invFrac + dist;
  verts[3].coord.y = (float)(line->frontsector->ceilingheight-line->frontsector->floorheight)*invFrac;

  verts[2].imgcoord = img->tl;
  verts[2].imgsize.x = img->width;
  verts[2].imgsize.y = img->height;
  verts[2].flags = 0;

  gl3_AddQuad(verts, GL3_BUF_WALLS);
}
