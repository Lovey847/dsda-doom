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
 *   OpenGL 3.3 shader handling
 */

#include "gl3_shader.h"
#include "gl3_texture.h"

#include "i_system.h"

// TODO: Store shader source code in dsda-doom.wad!
#define SHADERSRC(...) \
  "#version 330 core\n"\
  "#define LOC_INVERT 0\n"\
  "#define LOC_INIMGCOORD 1\n"\
  "#define LOC_INIMGSIZE 2\n"\
  "#define LOC_INCOORD 3\n"\
  "#define LOC_INFLAGS 4\n"\
  "#define PFLAG_TRANSMASK 15u\n"\
  #__VA_ARGS__

static const char lvShaderCode[] = SHADERSRC(
  layout(location = LOC_INVERT) in vec3 invert;
  layout(location = LOC_INFLAGS) in uint incol;

  layout(std140) uniform shaderdata_t {
    // Palette, premultiplied by number of translation tables
    uint palTimesTransTables;
  } shaderdata;

  flat out uint col;
  flat out uint palTimesTransTables;

  void main() {
    gl_Position = vec4(invert.xy, 0.0, 1.0);

    col = incol;
    palTimesTransTables = shaderdata.palTimesTransTables;
  }

  );

static const char lfShaderCode[] = SHADERSRC(
  flat in uint col;
  flat in uint palTimesTransTables;

  uniform sampler3D pal;

  out vec4 fragcolor;

  void main() {
    fragcolor = texelFetch(pal, ivec3(col, 0, palTimesTransTables), 0);
  }

  );

static const char pvShaderCode[] = SHADERSRC(
  layout(location = LOC_INVERT) in vec3 invert;
  layout(location = LOC_INIMGCOORD) in ivec2 inimgcoord;
  layout(location = LOC_INIMGSIZE) in ivec2 inimgsize;
  layout(location = LOC_INCOORD) in vec2 incoord;
  layout(location = LOC_INFLAGS) in uint inflags;

  layout(std140) uniform shaderdata_t {
    // Palette, premultiplied by number of translation tables
    uint palTimesTransTables;
  } shaderdata;

  flat out ivec2 imgcoord;
  flat out ivec2 imgsize;
  noperspective out vec2 coord;
  flat out uint flags;
  flat out uint palTimesTransTables;

  void main() {
    gl_Position = vec4(invert.xy, 0.0, 1.0);

    imgcoord = inimgcoord;
    imgsize = inimgsize;
    coord = incoord;
    flags = inflags;
    palTimesTransTables = shaderdata.palTimesTransTables;
  }

  );

static const char pfShaderCode[] = SHADERSRC(
  flat in ivec2 imgcoord;
  flat in ivec2 imgsize;
  noperspective in vec2 coord;
  flat in uint flags;
  flat in uint palTimesTransTables;

  uniform usampler2D tex;
  uniform sampler3D pal;

  out vec4 fragcolor;

  void main() {
    ivec2 c = ivec2(mod(coord, vec2(imgsize))) + imgcoord;
    uint ind = texelFetch(tex, c, 0).r;
    fragcolor = texelFetch(pal, ivec3(ind, 0, palTimesTransTables+(flags&PFLAG_TRANSMASK)), 0);
  }

  );

//////////////////////////////////
// OpenGL shader initialization
static const char *ShaderTypeStr(GLenum type) {
  switch (type) {
  case GL_VERTEX_SHADER: return "GL_VERTEX_SHADER";
  case GL_FRAGMENT_SHADER: return "GL_FRAGMENT_SHADER";
  }

  return "Unknown";
}

static GLuint CreateShader(const char *src, GLenum type) {
  GLint status;
  GLuint ret = GL3(gl3_glCreateShader(type));
  if (!ret) I_Error("CreateShader: Cannot create %s!\n", ShaderTypeStr(type));

  GL3(gl3_glShaderSource(ret, 1, &src, NULL));

  lprintf(LO_DEBUG, "CreateShader: Compiling %s\n", ShaderTypeStr(type));
  GL3(gl3_glCompileShader(ret));
  GL3(gl3_glGetShaderiv(ret, GL_COMPILE_STATUS, &status));

  if (!status) {
    char errbuf[512];
    GL3(gl3_glGetShaderInfoLog(ret, sizeof(errbuf), NULL, errbuf));

    GL3(gl3_glDeleteShader(ret));

    I_Error("CreateShader: %s error: %s", ShaderTypeStr(type), errbuf);
  }

  return ret;
}

static GLuint CreateProgram(const char *vertex, const char *fragment) {
  GLint status;
  GLuint v, f, ret = GL3(gl3_glCreateProgram());
  if (!ret) I_Error("CreateProgram: Cannot create program object!\n");

  v = CreateShader(vertex, GL_VERTEX_SHADER);
  f = CreateShader(fragment, GL_FRAGMENT_SHADER);

  GL3(gl3_glAttachShader(ret, v));
  GL3(gl3_glAttachShader(ret, f));

  lprintf(LO_DEBUG, "CreateProgram: Linking...\n");
  GL3(gl3_glLinkProgram(ret));

  GL3(gl3_glDetachShader(ret, v));
  GL3(gl3_glDetachShader(ret, f));
  GL3(gl3_glDeleteShader(v));
  GL3(gl3_glDeleteShader(f));

  GL3(gl3_glGetProgramiv(ret, GL_LINK_STATUS, &status));

  if (!status) {
    char errbuf[512];
    GL3(gl3_glGetProgramInfoLog(ret, sizeof(errbuf), NULL, errbuf));

    GL3(gl3_glDeleteProgram(ret));

    I_Error("CreateProgram: Link error: %s", errbuf);
  }

  return ret;
}

//////////////////////////
// Shader handling

gl3_shader_t gl3_shaders[GL3_SHADER_COUNT];

void gl3_InitShaders(void) {
  GLint u;

  gl3_shaders[GL3_SHADER_LINE].program = CreateProgram(lvShaderCode, lfShaderCode);

  // Set shader uniforms
//GL3(gl3_glUseProgram(gl3_shaders[GL3_SHADER_LINE].program));

//u = GL3(gl3_glGetUniformLocation(gl3_shaders[GL3_SHADER_LINE].program, "pal"));
//GL3(gl3_glUniform1i(u, 0));

  gl3_shaders[GL3_SHADER_PATCH].program = CreateProgram(pvShaderCode, pfShaderCode);

  // Set shader uniforms
  GL3(gl3_glUseProgram(gl3_shaders[GL3_SHADER_PATCH].program));

//u = GL3(gl3_glGetUniformLocation(gl3_shaders[GL3_SHADER_PATCH].program, "pal"));
//GL3(gl3_glUniform1i(u, 0));

  u = GL3(gl3_glGetUniformLocation(gl3_shaders[GL3_SHADER_PATCH].program, "tex"));
  GL3(gl3_glUniform1i(u, 1));

  I_AtExit(gl3_DeleteShaders, true);
}

void gl3_DeleteShaders(void) {
  GL3(gl3_glDeleteProgram(gl3_shaders[GL3_SHADER_PATCH].program));
}
