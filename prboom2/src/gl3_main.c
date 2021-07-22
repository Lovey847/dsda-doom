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

#include "lprintf.h"
#include "i_video.h"
#include "w_wad.h"

// OpenGL error code
int gl3_errno;

// OpenGL implementation information
int gl3_GL_MAX_TEXTURE_SIZE;
int gl3_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT;

// TODO: Implement error checking!
void gl3_Init(int width, int height) {
  const char *vendor, *renderer, *version, *glslVer;
  int i;

  // Load extension functions
  if (!gl3_InitOpenGL()) I_Error("Couldn't load extension functions!");

  // Set clear color
  GL3(glClearColor(1.f, 0.f, 0.f, 1.f));

  // Log opengl information
  vendor = GL3(glGetString(GL_VENDOR));
  renderer = GL3(glGetString(GL_RENDERER));
  version = GL3(glGetString(GL_VERSION));
  glslVer = GL3(glGetString(GL_SHADING_LANGUAGE_VERSION));

  lprintf(LO_INFO,
          "gl3_Init: OpenGL context information:\n"
          "            Vendor:       %s\n"
          "            Renderer:     %s\n"
          "            Version:      %s\n"
          "            GLSL Version: %s\n",

          vendor, renderer, version, glslVer);

  // Get implementation values
  GL3(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl3_GL_MAX_TEXTURE_SIZE));
  gl3_GL_MAX_TEXTURE_SIZE >>= 2;
  GL3(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &gl3_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT));
  lprintf(LO_INFO,
          "gl3_Init: OpenGL implementation information:\n"
          "            GL_MAX_TEXTURE_SIZE: %d\n"
          "            GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT: %d\n",

          gl3_GL_MAX_TEXTURE_SIZE, gl3_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);

  // Create textures
  gl3_InitTextures();
}

void gl3_Start(void) {
  glClear(GL_COLOR_BUFFER_BIT);
}

void gl3_Finish(void) {
  SDL_GL_SwapWindow(sdl_window);
}

void gl3_FillRect(int scrn, int x, int y, int width, int height, byte color) {

}

void gl3_DrawBackground(const char *flatname, int n) {

}

void gl3_FillFlat(int lump, int n, int x, int y, int width, int height,
                  enum patch_translation_e flags)
{

}

void gl3_FillPatch(int lump, int n, int x, int y, int width, int height,
                   enum patch_translation_e flags)
{

}

void gl3_DrawNumPatch(int x, int y, int scrn, int lump, int cm,
                      enum patch_translation_e flags)
{

}

void gl3_DrawNumPatchPrecise(float x, float y, int scrn, int lump, int cm,
                             enum patch_translation_e flags)
{

}

void gl3_PlotPixel(int scrn, int x, int y, byte color) {

}

void gl3_PlotPixelWu(int scr, int x, int y, byte color, int weight) {

}

void gl3_DrawLine(fline_t *fl, int color) {

}
