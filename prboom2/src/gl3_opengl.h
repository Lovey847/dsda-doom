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

// Extension function definition
// Uses versatile DEFEXTFUNC(_type, _name, ...) macro to easily define a list of all
// extension functions
//
// Make sure to #undef DEFEXTFUNC after using this macro
#define GL3_EXTFUNCS                                                    \
  /* Shaders */                                                         \
  DEFEXTFUNC(GLuint, glCreateShader, GLenum)                            \
  DEFEXTFUNC(void, glShaderSource,                                      \
             GLuint, GLsizei, const GLchar**, const GLint*)             \
  DEFEXTFUNC(void, glCompileShader, GLuint)                             \
  DEFEXTFUNC(void, glGetShaderiv, GLuint, GLenum, GLint*)               \
  DEFEXTFUNC(void, glGetShaderInfoLog,                                  \
             GLuint, GLsizei, GLsizei*, GLchar*)                        \
  DEFEXTFUNC(void, glDeleteShader, GLuint)                              \
  DEFEXTFUNC(GLuint, glCreateProgram, void)                             \
  DEFEXTFUNC(void, glAttachShader, GLuint, GLuint)                      \
  DEFEXTFUNC(void, glLinkProgram, GLuint)                               \
  DEFEXTFUNC(void, glGetProgramiv, GLuint, GLenum, GLint*)              \
  DEFEXTFUNC(void, glGetProgramInfoLog,                                 \
             GLuint, GLsizei, GLsizei*, GLchar*)                        \
  DEFEXTFUNC(void, glDetachShader, GLuint, GLuint)                      \
  DEFEXTFUNC(void, glDeleteProgram, GLuint)                             \
  DEFEXTFUNC(void, glUseProgram, GLuint)                                \
                                                                        \
  /* Shader uniforms */                                                 \
  DEFEXTFUNC(GLint, glGetUniformLocation, GLuint, const GLchar*)        \
  DEFEXTFUNC(void, glUniform1i, GLint, GLint)                           \
  DEFEXTFUNC(GLuint, glGetUniformBlockIndex, GLuint, const GLchar*)     \
  DEFEXTFUNC(void, glUniformBlockBinding, GLuint, GLuint, GLuint)       \
                                                                        \
  /* Vertex buffers */                                                  \
  DEFEXTFUNC(void, glGenVertexArrays, GLsizei, GLuint*)                 \
  DEFEXTFUNC(void, glBindVertexArray, GLuint)                           \
  DEFEXTFUNC(void, glGenBuffers, GLsizei, GLuint*)                      \
  DEFEXTFUNC(void, glBindBuffer, GLenum, GLuint)                        \
  DEFEXTFUNC(void, glBufferData,                                        \
             GLenum, GLsizeiptr, const void*, GLenum)                   \
  DEFEXTFUNC(void, glVertexAttribPointer,                               \
             GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)    \
  DEFEXTFUNC(void, glEnableVertexAttribArray, GLuint)                   \
  DEFEXTFUNC(void, glDeleteBuffers, GLsizei, const GLuint*)             \
  DEFEXTFUNC(void, glDeleteVertexArrays, GLsizei, const GLuint*)        \
  DEFEXTFUNC(void, glBufferSubData,                                     \
             GLenum, GLintptr, GLsizeiptr, const void*)                 \
  DEFEXTFUNC(void, glBindBufferRange,                                   \
             GLenum, GLuint, GLuint, GLintptr, GLsizeiptr)


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
dboolean gl3_InitOpenGL(void);

#endif //_GL3_OPENGL_H
