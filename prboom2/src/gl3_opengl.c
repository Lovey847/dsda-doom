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
 *   OpenGL 3.3 function loader
 */

#include "gl3_opengl.h"
#include "lprintf.h"

// Function pointer definitions
#define DEFEXTFUNC(_type, _name, ...)           \
  gl3_ ## _name ## _t gl3_ ## _name = NULL;
GL3_EXTFUNCS;
#undef DEFEXTFUNC

dboolean gl3_InitOpenGL(void)
{
  // Load functions
#define DEFEXTFUNC(_type, _name, ...)                           \
  if (!gl3_ ## _name) {                                         \
    gl3_ ## _name = SDL_GL_GetProcAddress(#_name);              \
    if (!gl3_ ## _name) return false;                           \
  }
  GL3_EXTFUNCS;
#undef DEFEXTFUNC

  return true;
}
