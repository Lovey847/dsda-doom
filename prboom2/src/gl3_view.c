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
  gl3_shaderdata.projmat[2][1] = (float)(SCREENHEIGHT - centery*2)/(float)SCREENHEIGHT;
  gl3_shaderdata.projmat[2][2] = (farclip+nearclip*2.f)/farclip;
  gl3_shaderdata.projmat[3][2] = -nearclip*2.f;
  gl3_shaderdata.projmat[2][3] = 1.f;
  gl3_shaderdata.projmat[3][3] = 0.f;
}

// Draw single wall from line
static void gl3_DrawWallPart(line_t *l, side_t *s, const gl3_img_t *img,
                             float dist, float x1, float y1, float x2, float y2,
                             float floorheight, float ceilingheight,
                             float xoffset, float yoffset)
{
  // Quad vertices
  gl3_vert_t verts[4];

  verts[0].x = x1;
  verts[0].y = ceilingheight;
  verts[0].z = y1;
  verts[0].coord.x = xoffset;
  verts[0].coord.y = yoffset;

  verts[1].x = x2;
  verts[1].y = ceilingheight;
  verts[1].z = y2;
  verts[1].coord.x = dist + xoffset;
  verts[1].coord.y = yoffset;

  verts[2].x = x1;
  verts[2].y = floorheight;
  verts[2].z = y1;
  verts[2].coord.x = xoffset;
  verts[2].coord.y = ceilingheight-floorheight + yoffset;

  verts[3].x = x2;
  verts[3].y = floorheight;
  verts[3].z = y2;
  verts[3].coord.x = dist + xoffset;
  verts[3].coord.y = ceilingheight-floorheight + yoffset;

  // Flat fragment properties
  verts[2].imgcoord = img->tl;
  verts[2].imgsize.x = img->width;
  verts[2].imgsize.y = img->height;
  verts[2].flags = 0;

  gl3_AddQuad(verts, GL3_BUF_WALLS);
}

void gl3_DrawWall(seg_t *line, mobj_t *player) {
  const gl3_img_t *top, *mid, *bottom;

  line_t *l = line->linedef;
  side_t *s = line->sidedef;
  float x1, y1, x2, y2;
  float backfloorheight, backceilingheight;
  float floorheight, ceilingheight;
  float dx, dy, dist;
  float xoffset, yoffset;

  top = gl3_GetWall(s->toptexture);
  mid = gl3_GetWall(s->midtexture);
  bottom = gl3_GetWall(s->bottomtexture);

  x1 = (float)line->v1->px*invFrac;
  y1 = (float)line->v1->py*invFrac;
  x2 = (float)line->v2->px*invFrac;
  y2 = (float)line->v2->py*invFrac;

  floorheight = (float)line->frontsector->floorheight*invFrac;
  ceilingheight = (float)line->frontsector->ceilingheight*invFrac;

  dx = x1-x2;
  dy = y1-y2;
  dist = sqrtf(dx*dx + dy*dy);

  xoffset = (float)(s->textureoffset+line->offset)*invFrac;
  yoffset = (float)s->rowoffset*invFrac;

  //////////////////////////////////
  // HOW RW_SCALE WORKS:
  //   First, R_StoreWallRange projects a line orthogonal to
  //   the linedef, onto the linedef (the closest possible distance you could have to it),
  //   then in R_ScaleFromGlobalAngle, the projected distance is
  //   "anti-projected" by _dividing_ by
  //   the cosine of the angle it wants to project onto the line
  //   with, then to get the depth of that distance, the full distance
  //   is multiplied with the cosine of the direction to the
  //   point it's projecting to, then it divides half the viewwidth by
  //   the depth.
  //
  //   The reason why it looks so confusing is because it reorders the operations,
  //   instead of dividing the denominator, multiplying the denominator, then
  //   dividing the numerator by the denominator, it multiplies the numerator by
  //   what the denominator would be divided by (same effect), multiplies the denomator,
  //   then divides the numerator by the denominator.
  //
  //   I hope this makes sense.

  // One sided line
  if (!line->backsector) {
    float yoff = yoffset;
    if (l->flags&ML_DONTPEGBOTTOM) yoff += floorheight-ceilingheight;

    gl3_DrawWallPart(l, s, mid, dist, x1, y1, x2, y2,
                     floorheight, ceilingheight,
                     xoffset, yoff);
  } else { // Two sided line
    backfloorheight = (float)line->backsector->floorheight*invFrac;
    backceilingheight = (float)line->backsector->ceilingheight*invFrac;

    if (backceilingheight < ceilingheight) {
      float yoff = yoffset;
      if (!(l->flags&ML_DONTPEGTOP)) yoff += backceilingheight-ceilingheight;

      gl3_DrawWallPart(l, s, top, dist, x1, y1, x2, y2,
                       backceilingheight, ceilingheight,
                       xoffset, yoff);
    }
    if (floorheight < backfloorheight) {
      float yoff = yoffset;
      if (l->flags & ML_DONTPEGBOTTOM) yoff += ceilingheight-backfloorheight;

      gl3_DrawWallPart(l, s, bottom, dist, x1, y1, x2, y2,
                       floorheight, backfloorheight,
                       xoffset, yoff);
    }
  }
}
