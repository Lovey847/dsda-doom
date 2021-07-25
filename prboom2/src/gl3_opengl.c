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

#include "gl3_main.h"
#include "lprintf.h"

#include <string.h>

// Function pointer definitions
#define DEFFUNC(_type, _name, ...)              \
  gl3_ ## _name ## _t gl3_ ## _name = NULL;
GL3_FUNCLIST;
#undef DEFFUNC

// Extension function pointer definitions
#define DEFFUNC(_type, _name, ...)                  \
  gl3_ext_ ## _name ## _t gl3_ext_ ## _name = NULL;
GL3_EXTFUNCLIST;
#undef DEFFUNC

// Whether extensions are supported or not
dboolean gl3_haveExt = false;

static dboolean IsExtensionSupported(const char *name) {
  GLuint ind;
  const char *extstr;

  GL3(glGetIntegerv(GL_NUM_EXTENSIONS, &ind));

  for (; ind--;) {
    extstr = GL3(gl3_glGetStringi(GL_EXTENSIONS, ind));
    if (!strcmp(name, extstr)) return true;
  }

  return false;
}

// Macro for loading functions
#define LOADFUNC(_func, _funcpre, _name)                          \
  if (!_funcpre ## _ ## _name) {                                  \
    _funcpre ## _ ## _name = SDL_GL_GetProcAddress(#_name);       \
    if (!_funcpre ## _ ## _name) {                                \
      lprintf(LO_WARN, #_func ": Failed to load %s!\n", #_name);  \
                                                                  \
      return false;                                               \
    }                                                             \
                                                                  \
    lprintf(LO_DEBUG, #_func ": Loaded %s\n", #_name);            \
  }

static dboolean InitExtensions(void) {
  // Load extension functions
#define DEFFUNC(_type, _name, ...)              \
  LOADFUNC(InitExtensions, gl3_ext, _name)
  GL3_EXTFUNCLIST;
#undef DEFFUNC

  return true;
}

dboolean gl3_InitOpenGL(void) {
  // Load functions
#define DEFFUNC(_type, _name, ...)              \
  LOADFUNC(gl3_InitOpenGL, gl3, _name)
  GL3_FUNCLIST;
#undef DEFFUNC

  // With glGetStringi, check if all extensions are supported
  if (IsExtensionSupported("GL_ARB_debug_output")) {
    lprintf(LO_INFO, "gl3_InitOpenGL: Loading extension functions\n");
    gl3_haveExt = InitExtensions();
  }

  return true;
}
