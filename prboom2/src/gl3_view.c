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

void gl3_DrawWall(seg_t *line, mobj_t *player) {
  static const float invFrac = 1.f/(float)FRACUNIT;

  // Quad vertices
  gl3_vert_t verts[4];

  // TODO: This is all a test, actually implement the shaders, matrices and math needed
  // to draw walls!
  line_t *l = line->linedef;
  side_t *s = line->sidedef;
  const gl3_img_t *img = gl3_GetWall(s->midtexture);
  float x1, y1, x2, y2, x3, y3, x4, y4;
  union {
    float f;
    unsigned i;
  } dir;

  x1 = (float)line->v1->x*invFrac;
  y1 = (float)line->v1->y*invFrac;
  x2 = (float)line->v2->x*invFrac;
  y2 = (float)line->v2->y*invFrac;

  // Get relative line position
  x1 -= (float)player->x*invFrac;
  y1 -= (float)player->y*invFrac;
  x2 -= (float)player->x*invFrac;
  y2 -= (float)player->y*invFrac;

  // Compute direction difference
  // This assumes it's in ieee754 float format
  dir.i = 0x3f800000 | (player->angle>>9);
  dir.f = (dir.f-1.f)*(3.14159265358979323f*2.f);

  x3 = x1*cosf(dir.f) + y1*sinf(dir.f);
  y3 = x1*-sinf(dir.f) + y1*cosf(dir.f);
  x4 = x2*cosf(dir.f) + y2*sinf(dir.f);
  y4 = x2*-sinf(dir.f) + y2*cosf(dir.f);

  verts[0].x = x3*(float)scaledviewwidth/(float)(SCREENWIDTH*SCREENWIDTH);
  verts[1].x = x4*(float)scaledviewwidth/(float)(SCREENWIDTH*SCREENWIDTH);
  verts[0].y = y3*(float)scaledviewwidth/(float)(SCREENWIDTH*SCREENWIDTH)*(float)SCREENWIDTH/(float)SCREENHEIGHT - (float)(viewwindowy+viewheight/2-SCREENHEIGHT/2)/(float)SCREENHEIGHT;
  verts[1].y = y4*(float)scaledviewwidth/(float)(SCREENWIDTH*SCREENWIDTH)*(float)SCREENWIDTH/(float)SCREENHEIGHT - (float)(viewwindowy+viewheight/2-SCREENHEIGHT/2)/(float)SCREENHEIGHT;
  verts[1].flags = 208;

  gl3_AddLine(verts);
}
