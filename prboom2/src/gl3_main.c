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
#include "i_video.h"
#include "lprintf.h"

void gl3_Init(int width, int height) {
  const char *vendor, *renderer, *version, *glslVer;

  // Load extension functions
  if (!gl3_InitOpenGL()) I_Error("Couldn't load extension functions!");

  // Set clear color
  glClearColor(1.f, 0.f, 0.f, 1.f);

  // Log opengl information
  vendor = glGetString(GL_VENDOR);
  renderer = glGetString(GL_RENDERER);
  version = glGetString(GL_VERSION);
  glslVer = glGetString(GL_SHADING_LANGUAGE_VERSION);

  lprintf(LO_INFO,
          "gl3_Init: OpenGL context information:\n"
          "            Vendor:       %s\n"
          "            Renderer:     %s\n"
          "            Version:      %s\n"
          "            GLSL Version: %s\n",

          vendor, renderer, version, glslVer);
}

void gl3_Start(void) {
  glClear(GL_COLOR_BUFFER_BIT);
}

void gl3_Finish(void) {
  SDL_GL_SwapWindow(sdl_window);
}

// TODO: Right now, these are wrappers for gld functions.
//       Implement these at some point!
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
