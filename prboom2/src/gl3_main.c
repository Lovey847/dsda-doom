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
#include "gl3_view.h"

#include "lprintf.h"
#include "i_video.h"
#include "w_wad.h"
#include "doomstat.h"
#include "r_main.h"
#include "dsda/palette.h"

// OpenGL debug message callback
#ifndef NDEBUG
static const char *sourceStr(GLenum source) {
  switch (source) {
  case GL_DEBUG_SOURCE_API_ARB: return "GL_DEBUG_SOURCE_API_ARB";
  case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: return "GL_DEBUG_SOURCE_SHADER_COMPILER_ARB";
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB: return "GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB";
  case GL_DEBUG_SOURCE_THIRD_PARTY_ARB: return "GL_DEBUG_SOURCE_THIRD_PARTY_ARB";
  case GL_DEBUG_SOURCE_APPLICATION_ARB: return "GL_DEBUG_SOURCE_APPLICATION_ARB";
  case GL_DEBUG_SOURCE_OTHER_ARB: return "GL_DEBUG_SOURCE_OTHER_ARB";
  }

  return "NONE";
}
static const char *typeStr(GLenum type) {
  switch (type) {
  case GL_DEBUG_TYPE_ERROR_ARB: return "GL_DEBUG_TYPE_ERROR_ARB";
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: return "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB";
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB: return "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB";
  case GL_DEBUG_TYPE_PERFORMANCE_ARB: return "GL_DEBUG_TYPE_PERFORMANCE_ARB";
  case GL_DEBUG_TYPE_PORTABILITY_ARB: return "GL_DEBUG_TYPE_PORTABILITY_ARB";
  case GL_DEBUG_TYPE_OTHER_ARB: return "GL_DEBUG_TYPE_OTHER_ARB";
  }

  return "NONE";
}
static const char *severityStr(GLenum severity) {
  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH_ARB: return "GL_DEBUG_SEVERITY_HIGH_ARB";
  case GL_DEBUG_SEVERITY_MEDIUM_ARB: return "GL_DEBUG_SEVERITY_MEDIUM_ARB";
  case GL_DEBUG_SEVERITY_LOW_ARB: return "GL_DEBUG_SEVERITY_LOW_ARB";
  }

  return "Notification";
}

static void APIENTRY ErrorCallback(GLenum source, GLenum type, GLuint id,
                                   GLenum severity, GLsizei length, const GLchar *msg,
                                   const GLvoid *userParam)
{
  OutputLevels o;
  switch (severity) {
  case GL_DEBUG_SEVERITY_LOW_ARB:
  case GL_DEBUG_SEVERITY_MEDIUM_ARB:
  case GL_DEBUG_SEVERITY_HIGH_ARB: o = LO_WARN; break;

  default: o = LO_DEBUG; break;
  }

  // NOTE: lprintf uses static data, is this thread save?
  lprintf(o,
          "OpenGL debug message:\n"
          "  Source: %s\n"
          "  Type: %s\n"
          "  ID: %u\n"
          "  Severity: %s\n"
          "  Message: \"%s\"\n",

          sourceStr(source), typeStr(type), id, severityStr(severity), msg);
}
#endif

// Report invalid lump nums
static void ReportInvalidPatch(int lump) {
  static int invalidpatches[32];
  static size_t invalidpatchcount = 0;

  size_t i;

  if (invalidpatchcount >= 32) return;

  for (i = 0; i < invalidpatchcount; ++i)
    if (invalidpatches[i] == lump) return;

  invalidpatches[invalidpatchcount++] = lump;
  lprintf(LO_WARN, "ReportInvalidPatch: Invalid patch %d!\n", lump);
}

static INLINE void ReportInvalidFlat(int flat) {
  ReportInvalidPatch(flat+firstflat);
}

// OpenGL error code
int gl3_errno;

// OpenGL implementation information
int gl3_GL_MAX_TEXTURE_SIZE;
int gl3_GL_MAX_3D_TEXTURE_SIZE;
int gl3_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT;

void gl3_Init(int width, int height) {
  const char *vendor, *renderer, *version, *glslVer;
  int i;

  // Load extension functions
  if (!gl3_InitOpenGL()) I_Error("Couldn't load extension functions!");

  // Enable debug output if we're in the debug build
#ifndef NDEBUG
  if (gl3_haveExt) {
    GL3(gl3_ext_glDebugMessageCallbackARB(ErrorCallback, NULL));

    // Enable low severity messages
    GL3(gl3_ext_glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE,
                                         GL_DONT_CARE, 0, NULL, true));
  }
#endif

  // Set clear color&depth
  GL3(glClearColor(0.f, 0.f, 0.f, 1.f));
  GL3(glClearDepth(1.f));

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
  GL3(glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &gl3_GL_MAX_3D_TEXTURE_SIZE));
  GL3(glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &gl3_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT));
  lprintf(LO_INFO,
          "gl3_Init: OpenGL implementation information:\n"
          "            GL_MAX_TEXTURE_SIZE: %d\n"
          "            GL_MAX_3D_TEXTURE_SIZE: %d\n"
          "            GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT: %d\n",

          gl3_GL_MAX_TEXTURE_SIZE, gl3_GL_MAX_3D_TEXTURE_SIZE,
          gl3_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);

  // Create textures
  gl3_InitTextures();

  // Create shaders
  gl3_InitShaders();

  // Create buffers
  gl3_InitBuffers(2048, 3072);

  // Enable alpha blending
  GL3(glEnable(GL_BLEND));
  GL3(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

  // Enable depth buffer
  GL3(glEnable(GL_DEPTH_TEST));
  GL3(glDepthFunc(GL_LEQUAL));
}

void gl3_Start(void) {
  lprintf(LO_DEBUG, "gl3_Start: Frame start\n\n");
  GL3(glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT));
}

void gl3_Finish(void) {
  gl3_FlushBuffers();
  SDL_GL_SwapWindow(sdl_window);
}

void gl3_SetPalette(int palette) {
  gl3_shaderdata.palTimesTransTables = palette*(CR_LIMIT+1);
}

void gl3_FillRect(int scrn, int x, int y, int width, int height, byte color) {
  gl3_vert_t verts[4] = {};
  const float two_over_width = 2.f/SCREENWIDTH;
  const float negative_two_over_height = -2.f/SCREENHEIGHT;
  dsda_playpal_t *ppd = dsda_PlayPalData();

  float xx = (float)x*two_over_width - 1.f;
  float yy = (float)y*negative_two_over_height + 1.f;
  float ex = xx + (float)width*two_over_width;
  float ey = yy + (float)height*negative_two_over_height;

  verts[0].x = xx;
  verts[0].y = yy;

  verts[1].x = ex;
  verts[1].y = yy;

  verts[2].x = xx;
  verts[2].y = ey;

  verts[3].x = ex;
  verts[3].y = ey;

  if (color == ppd->transparent) color = ppd->duplicate;
  verts[2].imgcoord = gl3_ColCoord(color);

  gl3_AddQuad(verts, GL3_BUF_PATCHES);
}

void gl3_DrawBackground(const char *flatname, int n) {
  gl3_FillFlat(R_FlatNumForName(flatname), n, 0, 0, SCREENWIDTH, SCREENHEIGHT, VPT_NONE);
}

void gl3_FillFlat(int lump, int n, int x, int y, int width, int height,
                  enum patch_translation_e flags)
{
  const gl3_img_t *img = gl3_GetFlat(lump);

  // Log invalid flats
  if (!img) ReportInvalidFlat(lump);
  else gl3_AddImage(img, x, y, width, height, CR_DEFAULT, flags);
}

void gl3_FillPatch(int lump, int n, int x, int y, int width, int height,
                   enum patch_translation_e flags)
{
  const gl3_img_t *img = gl3_GetPatch(lump);

  // Log invalid patches
  if (!img) ReportInvalidPatch(lump);
  else gl3_AddImage(img, x, y, width, height, CR_DEFAULT, flags);
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
  else gl3_AddImage(img, x, y, img->width, img->height, cm, flags);
}

void gl3_PlotPixel(int scrn, int x, int y, byte color) {
  gl3_FillRect(scrn, x, y, 1, 1, color);
}

void gl3_PlotPixelWu(int scr, int x, int y, byte color, int weight) {
  gl3_PlotPixel(scr, x, y, color);
}

void gl3_DrawLine(fline_t *fl, int color) {
  const float two_over_width = 2.f/(float)SCREENWIDTH;
  const float negative_two_over_height = -2.f/(float)SCREENHEIGHT;

  gl3_vert_t verts[2] = {};
  float sx, sy, ex, ey;
  float castx, casty;

  // Round pixel positions
  // Using non-float coordinates.
  sx = (float)fl->a.x+0.5f;
  sy = (float)fl->a.y+0.5f;
  ex = (float)fl->b.x+0.5f;
  ey = (float)fl->b.y+0.5f;

  // Cast ex and ey to next pixel
  if (fabs(sx-ex) > fabs(sy-ey)) {
    if (sx < ex) castx = 1.f;
    else castx = -1.f;

    casty = (ey-sy)/fabs(sx-ex);
  } else {
    castx = (ex-sx)/fabs(sy-ey);

    if (sy < ey) casty = 1.f;
    else casty = -1.f;
  }

  ex += castx;
  ey += casty;

  sx = sx*two_over_width-1.f;
  sy = sy*negative_two_over_height+1.f;
  ex = ex*two_over_width-1.f;
  ey = ey*negative_two_over_height+1.f;

  verts[0].x = sx;
  verts[0].y = sy;

  verts[1].x = ex;
  verts[1].y = ey;

  verts[1].flags = color;

  gl3_AddLine(verts);
}

void gl3_wipe_doMelt(int *y_lookup) {
  GL3(glClear(GL_COLOR_BUFFER_BIT));
}

void gl3_wipe_exitMelt(int ticks) {
}

void gl3_wipe_StartScreen(void) {
}

void gl3_wipe_EndScreen(void) {
}

void gl3_bsp_AddLine(seg_t *line) {
  // Render from the players POV
  mobj_t *p = players[displayplayer].mo;

  // Make sure wall should be drawn
  fixed_t ang1, ang2;

  ang1 = R_PointToPseudoAngle(line->v1->px, line->v1->py);
  ang2 = R_PointToPseudoAngle(line->v2->px, line->v2->py);

  // Don't draw back of lines
  if (ang1-ang2 >= ANG180) return;

  // Mark line as visible
  // TODO: Now I know why gl_clipper.c exists,
  // it's for the automap!
  // Properly clip these things! (sucks because
  // I can't really take advantage of the automap)
  line->linedef->flags |= ML_MAPPED;

  gl3_DrawWall(line, p);
}
