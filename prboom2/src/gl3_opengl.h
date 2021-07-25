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

#include "doomtype.h"

// Loaded functions
// Uses versatile DEFFUNC(_type, _name, ...) macro to easily define a list of all
// extension functions
//
// Make sure to #undef DEFFUNC after using this macro
#define GL3_FUNCLIST                                                    \
  /* Shaders */                                                         \
  DEFFUNC(GLuint, glCreateShader, GLenum)                               \
  DEFFUNC(void, glShaderSource,                                         \
          GLuint, GLsizei, const GLchar**, const GLint*)                \
  DEFFUNC(void, glCompileShader, GLuint)                                \
  DEFFUNC(void, glGetShaderiv, GLuint, GLenum, GLint*)                  \
  DEFFUNC(void, glGetShaderInfoLog,                                     \
          GLuint, GLsizei, GLsizei*, GLchar*)                           \
  DEFFUNC(void, glDeleteShader, GLuint)                                 \
  DEFFUNC(GLuint, glCreateProgram, void)                                \
  DEFFUNC(void, glAttachShader, GLuint, GLuint)                         \
  DEFFUNC(void, glLinkProgram, GLuint)                                  \
  DEFFUNC(void, glGetProgramiv, GLuint, GLenum, GLint*)                 \
  DEFFUNC(void, glGetProgramInfoLog,                                    \
          GLuint, GLsizei, GLsizei*, GLchar*)                           \
  DEFFUNC(void, glDetachShader, GLuint, GLuint)                         \
  DEFFUNC(void, glDeleteProgram, GLuint)                                \
  DEFFUNC(void, glUseProgram, GLuint)                                   \
                                                                        \
  /* Shader uniforms */                                                 \
  DEFFUNC(GLint, glGetUniformLocation, GLuint, const GLchar*)           \
  DEFFUNC(void, glUniform1i, GLint, GLint)                              \
  DEFFUNC(GLuint, glGetUniformBlockIndex, GLuint, const GLchar*)        \
  DEFFUNC(void, glUniformBlockBinding, GLuint, GLuint, GLuint)          \
                                                                        \
  /* Vertex buffers */                                                  \
  DEFFUNC(void, glGenVertexArrays, GLsizei, GLuint*)                    \
  DEFFUNC(void, glBindVertexArray, GLuint)                              \
  DEFFUNC(void, glGenBuffers, GLsizei, GLuint*)                         \
  DEFFUNC(void, glBindBuffer, GLenum, GLuint)                           \
  DEFFUNC(void, glBufferData,                                           \
          GLenum, GLsizeiptr, const void*, GLenum)                      \
  DEFFUNC(void, glVertexAttribPointer,                                  \
          GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)       \
  DEFFUNC(void, glEnableVertexAttribArray, GLuint)                      \
  DEFFUNC(void, glDeleteBuffers, GLsizei, const GLuint*)                \
  DEFFUNC(void, glDeleteVertexArrays, GLsizei, const GLuint*)           \
  DEFFUNC(void, glBufferSubData,                                        \
          GLenum, GLintptr, GLsizeiptr, const void*)                    \
  DEFFUNC(void, glBindBufferRange,                                      \
          GLenum, GLuint, GLuint, GLintptr, GLsizeiptr)                 \
                                                                        \
  /* Misc. functions */                                                 \
  DEFFUNC(const GLubyte*, glGetStringi, GLenum, GLuint)

// Extension function list
#define GL3_EXTFUNCLIST                                               \
  /* ARB_debug_output */                                              \
  DEFFUNC(void, glDebugMessageControlARB,                             \
          GLenum, GLenum, GLenum, GLsizei, const GLuint*, GLboolean)  \
  DEFFUNC(void, glDebugMessageInsertARB,                              \
          GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*)     \
  DEFFUNC(void, glDebugMessageCallbackARB,                            \
          GLDEBUGPROCARB, const GLvoid*)                              \
  DEFFUNC(GLuint, glGetDebugMessageLogARB,                            \
          GLuint, GLsizei, GLenum*, GLenum*, GLuint*, GLenum*,        \
          GLsizei*, GLchar*)                                          \
  DEFFUNC(void, glGetPointerv, GLenum, GLvoid**)

// Loaded function typedefs
// I don't use the usual types since I need names
// that can be constructed from _name, don't wanna add
// a proc parameter to DEFFUNC
#define DEFFUNC(_type, _name, ...)                            \
  typedef _type (APIENTRYP gl3_ ## _name ## _t)(__VA_ARGS__);
GL3_FUNCLIST;
#undef DEFFUNC

// Loaded function pointers
#define DEFFUNC(_type, _name, ...)              \
  extern gl3_ ## _name ## _t gl3_ ## _name;
GL3_FUNCLIST;
#undef DEFFUNC

// Extension function typedefs
#define DEFFUNC(_type, _name, ...)                                      \
  typedef _type (APIENTRYP gl3_ext_ ## _name ## _t)(__VA_ARGS__);
GL3_EXTFUNCLIST;
#undef DEFFUNC

// Extension function pointers
#define DEFFUNC(_type, _name, ...)                            \
  extern gl3_ext_ ## _name ## _t gl3_ext_ ## _name;
GL3_EXTFUNCLIST;
#undef DEFFUNC

// Whether extensions are supported or not
extern dboolean gl3_haveExt;

// Load functions and initialize gl3_haveExt
dboolean gl3_InitOpenGL(void);

#endif //_GL3_OPENGL_H
