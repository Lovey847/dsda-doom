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

#ifndef _GL3_OPENGL_H
#define _GL3_OPENGL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL.h>
#include <SDL_opengl.h>

// Dunno if I wanna include the opengl header when including SDL_opengl or not

// Descriptive #if 0
#ifdef unsure_whether_to_include_gl_h_or_not

// Include OpenGL header
#if defined(__MACOSX__)
#include <OpenGL/gl.h>

#elif defined(__MACOS__)
#include <gl.h>

#else
#include <GL/gl.h>

#endif

#endif // unsure_whether_to_include_gl_h_or_not

#include "doomtype.h"

// Extension function definition
// Uses versatile DEFEXTFUNC(_type, _name, ...) macro to easily define a list of all
// extension functions
//
// Make sure to #undef DEFEXTFUNC after using this macro
#define GL3_EXTFUNCS \
  DEFEXTFUNC(void, glDrawElements, GLenum, GLsizei, GLenum, const void*)

// Extension function typedefs
// I don't use the usual types since I need names
// that can be constructed from _name, don't wanna add
// a proc parameter to DEFEXTFUNC
#define DEFEXTFUNC(_type, _name, ...)                         \
  typedef _type (APIENTRYP gl3_ ## _name ## _t)(__VA_ARGS__);
GL3_EXTFUNCS;
#undef DEFEXTFUNC

// Extension function pointers
#define DEFEXTFUNC(_type, _name, ...)           \
  extern gl3_ ## _name ## _t gl3_ ## _name;
GL3_EXTFUNCS;
#undef DEFEXTFUNC

// Load functions
void gl3_InitOpenGL(void);

#endif //_GL3_OPENGL_H
