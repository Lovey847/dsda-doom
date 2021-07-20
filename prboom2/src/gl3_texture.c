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
 *   Main OpenGL 3.3 implementation
 */

#include "gl3_main.h"
#include "gl3_texture.h"

#include "i_system.h"

GLuint gl3_textures[GL3_TEX_COUNT];

void gl3_InitTextures(void) {
  int i;
  
  // Initialize textures
  glGenTextures(GL3_TEX_COUNT, gl3_textures);

  for (i = 0; i < GL3_TEX_COUNT; ++i) {
    glActiveTexture(GL_TEXTURE0+i);
    glBindTexture(GL_TEXTURE_2D, gl3_textures[i]);

    // Set texture parameters
    // TODO: Make filtering an option at some point!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create texture image
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 gl3_GL_MAX_TEXTURE_SIZE, gl3_GL_MAX_TEXTURE_SIZE,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  }

  // TODO: When OpenGL 3.3 is fully implemented, we must _actually_ free up
  //       OpenGL resources when switching video modes, instead of doing it
  //       at exit, since VID_MODEGL and VID_MODEGL3 share resources!
  I_AtExit(gl3_DeleteTextures, true);
}

void gl3_DeleteTextures(void) {
  glDeleteTextures(GL3_TEX_COUNT, gl3_textures);
}
