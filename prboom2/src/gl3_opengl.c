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
#define DEFFUNC(_type, _name, ...)           \
  gl3_ ## _name ## _t gl3_ ## _name = NULL;
GL3_FUNCLIST;
#undef DEFFUNC

dboolean gl3_InitOpenGL(void)
{
  // Load functions
#define DEFFUNC(_type, _name, ...)                                   \
  if (!gl3_ ## _name) {                                                 \
    gl3_ ## _name = SDL_GL_GetProcAddress(#_name);                      \
    if (!gl3_ ## _name) {                                               \
      lprintf(LO_INFO, "gl3_InitOpenGL: Failed to load %s!\n", #_name); \
      return false;                                                     \
    }                                                                   \
                                                                        \
    lprintf(LO_INFO, "gl3_InitOpenGL: Loaded %s\n", #_name);            \
  }
  GL3_FUNCLIST;
#undef DEFFUNC

  return true;
}
