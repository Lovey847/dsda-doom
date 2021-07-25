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
#include "gl3_shader.h"
#include "gl3_buffer.h"

#include "lprintf.h"
#include "i_video.h"
#include "w_wad.h"

// Report invalid patch
static void ReportInvalidPatch(int lump) {
  static int invalidpatches[32];
  static size_t invalidpatchcount = 0;

  size_t i;

  if (invalidpatchcount >= 32) return;

  for (i = 0; i < invalidpatchcount; ++i)
    if (invalidpatches[i] == lump) return;

  invalidpatches[invalidpatchcount++] = lump;
  lprintf(LO_INFO, "ReportInvalidPatch: Invalid patch %d!\n", lump);
}

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
  GL3(glClearColor(0.f, 0.f, 0.f, 1.f));

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

  // Create shaders
  gl3_InitShaders();

  // Create buffers
  gl3_InitBuffers(2048, 3072);

  GL3(gl3_glUseProgram(gl3_shaders[GL3_SHADER_PATCH].program));

  // Enable alpha blending
  GL3(glEnable(GL_BLEND));
  GL3(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

  // Setup uniform block data
  gl3_shaderdata.blend[0] = 1.f;
  gl3_shaderdata.blend[1] = 1.f;
  gl3_shaderdata.blend[2] = 1.f;
  gl3_shaderdata.blend[3] = 1.f;
}

void gl3_Start(void) {
  GL3(glClear(GL_COLOR_BUFFER_BIT));
}

void gl3_Finish(void) {
  gl3_DrawBuffers();
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
  gl3_DrawNumPatchPrecise(x, y, scrn, lump, cm, flags);
}

void gl3_DrawNumPatchPrecise(float x, float y, int scrn, int lump, int cm,
                             enum patch_translation_e flags)
{
  const gl3_img_t *img = gl3_GetPatch(lump);

  // Log invalid patches
  if (!img) ReportInvalidPatch(lump);
  else gl3_AddImage(img, x, y, flags);
}

void gl3_PlotPixel(int scrn, int x, int y, byte color) {

}

void gl3_PlotPixelWu(int scr, int x, int y, byte color, int weight) {

}

void gl3_DrawLine(fline_t *fl, int color) {

}
