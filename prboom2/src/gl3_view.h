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

#ifndef _GL3_VIEW_H
#define _GL3_VIEW_H

#include "gl3_main.h"

#include "r_defs.h"

// Set player view matrices
void gl3_SetViewMatrices(mobj_t *player);

// Draw wall from player's POV
void gl3_DrawWall(seg_t *line, mobj_t *player);

#endif //_GL3_VIEW_H
