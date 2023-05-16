/* Emacs style mode select   -*- C -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *
 *---------------------------------------------------------------------
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include <math.h>
#include "doomstat.h"
#include "v_video.h"
#include "gl_opengl.h"
#include "gl_intern.h"
#include "r_main.h"
#include "w_wad.h"
#include "i_system.h"
#include "r_bsp.h"
#include "lprintf.h"
#include "m_file.h"
#include "e6y.h"
#include "r_things.h"
#include "doomdef.h"

// Indexed lighting shader uniform bindings
typedef struct shdr_indexed_unif_s
{
  int lightlevel_index; // float
} shdr_indexed_unif_t;

// Fuzz shader uniform bindings
typedef struct shdr_fuzz_unif_s
{
  // (vec2) sprite texture dimensions
  int tex_d_index;
  // (float) ratio of screen resolution to fuzz resolution
  int ratio_index;
  // (float) random seed
  int seed_index;
} shdr_fuzz_unif_t;

static GLShader *sh_indexed = NULL;
static shdr_indexed_unif_t indexed_unifs;
static GLShader *sh_fuzz = NULL;
static shdr_fuzz_unif_t fuzz_unifs;
static GLShader *active_shader = NULL;

static GLShader* gld_LoadShader(const char *vpname, const char *fpname);

static void get_indexed_shader_bindings()
{
  int idx;

  indexed_unifs.lightlevel_index = GLEXT_glGetUniformLocationARB(sh_indexed->hShader, "lightlevel");

  GLEXT_glUseProgramObjectARB(sh_indexed->hShader);

  idx = GLEXT_glGetUniformLocationARB(sh_indexed->hShader, "tex");
  GLEXT_glUniform1iARB(idx, 0);

  idx = GLEXT_glGetUniformLocationARB(sh_indexed->hShader, "colormap");
  GLEXT_glUniform1iARB(idx, 2);

  GLEXT_glUseProgramObjectARB(0);
}

static void get_fuzz_shader_bindings()
{
  int idx;

  fuzz_unifs.tex_d_index = GLEXT_glGetUniformLocationARB(sh_fuzz->hShader, "tex_d");
  fuzz_unifs.ratio_index = GLEXT_glGetUniformLocationARB(sh_fuzz->hShader, "ratio");
  fuzz_unifs.seed_index = GLEXT_glGetUniformLocationARB(sh_fuzz->hShader, "seed");

  GLEXT_glUseProgramObjectARB(sh_fuzz->hShader);

  idx = GLEXT_glGetUniformLocationARB(sh_fuzz->hShader, "tex");
  GLEXT_glUniform1iARB(idx, 0);

  GLEXT_glUseProgramObjectARB(0);
}

void glsl_Init(void)
{
  sh_indexed = gld_LoadShader("glvp", "glfp_idx");
  get_indexed_shader_bindings();

  sh_fuzz = gld_LoadShader("glvp", "glfp_fuzz");
  get_fuzz_shader_bindings();
}

static int ReadLump(const char *filename, const char *lumpname, char **buffer)
{
  int size;

  size = M_ReadFileToString(filename, buffer);

  if (size < 0)
  {
    const unsigned char *data;
    int lump;
    char name[9];
    char* p;

    strncpy(name, lumpname, 9);
    name[8] = 0;
    for(p = name; *p; p++)
      *p = toupper(*p);

    lump = W_CheckNumForName2(name, ns_prboom);

    if (lump != LUMP_NOT_FOUND)
    {
      size = W_LumpLength(lump);
      data = W_LumpByNum(lump);
      *buffer = Z_Calloc(1, size + 1);
      memcpy (*buffer, data, size);
      (*buffer)[size] = 0;
    }
  }

  return size;
}

static GLShader* gld_LoadShader(const char *vpname, const char *fpname)
{
#define buffer_size 2048
  int linked;
  char buffer[buffer_size];
  char *vp_data = NULL;
  char *fp_data = NULL;
  int vp_size, fp_size;
  size_t vp_fnlen, fp_fnlen;
  char *filename = NULL;
  GLShader* shader = NULL;

  vp_fnlen = snprintf(NULL, 0, "%s/shaders/%s.txt", I_DoomExeDir(), vpname);
  fp_fnlen = snprintf(NULL, 0, "%s/shaders/%s.txt", I_DoomExeDir(), fpname);
  filename = Z_Malloc(MAX(vp_fnlen, fp_fnlen) + 1);

  sprintf(filename, "%s/shaders/%s.txt", I_DoomExeDir(), vpname);
  vp_size = ReadLump(filename, vpname, &vp_data);

  sprintf(filename, "%s/shaders/%s.txt", I_DoomExeDir(), fpname);
  fp_size = ReadLump(filename, fpname, &fp_data);

  if (vp_data && fp_data)
  {
    shader = Z_Calloc(1, sizeof(GLShader));

    shader->hVertProg = GLEXT_glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    shader->hFragProg = GLEXT_glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

    // I think this is fixable with temporary variables and exchanging data around
    // Not sure on the right code to avoid adding extra variables, so ignoring...
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
    GLEXT_glShaderSourceARB(shader->hVertProg, 1, &vp_data, &vp_size);
    GLEXT_glShaderSourceARB(shader->hFragProg, 1, &fp_data, &fp_size);
    #pragma GCC diagnostic pop

    GLEXT_glCompileShaderARB(shader->hVertProg);
    GLEXT_glCompileShaderARB(shader->hFragProg);

    shader->hShader = GLEXT_glCreateProgramObjectARB();

    GLEXT_glAttachObjectARB(shader->hShader, shader->hVertProg);
    GLEXT_glAttachObjectARB(shader->hShader, shader->hFragProg);

    GLEXT_glLinkProgramARB(shader->hShader);

    GLEXT_glGetInfoLogARB(shader->hShader, buffer_size, NULL, buffer);

    GLEXT_glGetObjectParameterivARB(shader->hShader, GL_OBJECT_LINK_STATUS_ARB, &linked);

    if (linked)
    {
      lprintf(LO_DEBUG, "gld_LoadShader: Shader \"%s+%s\" compiled OK: %s\n", vpname, fpname, buffer);
    }
    else
    {
      lprintf(LO_ERROR, "gld_LoadShader: Error compiling shader \"%s+%s\": %s\n", vpname, fpname, buffer);
      Z_Free(shader);
      shader = NULL;
    }
  }

  Z_Free(filename);
  Z_Free(vp_data);
  Z_Free(fp_data);

  if (!shader)
  {
    I_Error("Failed to load shader %s, %s", vpname, fpname);
  }

  return shader;
}

// TODO: replace the active_shader variable with a stack;
// a few places need to temporarily disable or switch the
// current active shader (e.g. fuzz, gld_FillBlock, etc.)
// and the current management around this is very brittle.
// being able to push & pop to a stack would be quite nice.

void glsl_SetActiveShader(GLShader *shader)
{
  if (shader != active_shader)
  {
    GLEXT_glUseProgramObjectARB((shader ? shader->hShader : 0));
    active_shader = shader;
  }
}

void glsl_SuspendActiveShader(void)
{
  if (active_shader)
    GLEXT_glUseProgramObjectARB(0);
}

void glsl_ResumeActiveShader(void)
{
  if (active_shader)
    GLEXT_glUseProgramObjectARB(active_shader->hShader);
}

void glsl_SetMainShaderActive()
{
  glsl_SetActiveShader(sh_indexed);
}

void glsl_SetFuzzShaderActive(int tic, int sprite, int width, int height, float ratio)
{
  // Large integers converted to float can lose precision, causing
  // problems in the shader.  Since the tic and sprite count are just
  // used for randomness, munge them down and convert to float with
  // double precision here
  const int factor = 1103515245;
  int seed = 0xD00D;

  seed = seed * factor + tic;
  seed = seed * factor + sprite;
  seed *= factor;

  if (active_shader != sh_fuzz)
  {
    GLEXT_glUseProgramObjectARB(sh_fuzz->hShader);
    active_shader = sh_fuzz;
  }

  GLEXT_glUniform2fARB(fuzz_unifs.tex_d_index, width, height);
  GLEXT_glUniform1fARB(fuzz_unifs.ratio_index, ratio);
  GLEXT_glUniform1fARB(fuzz_unifs.seed_index, (double) seed / INT_MAX);
}

void glsl_SetFuzzShaderInactive()
{
  if (active_shader == sh_fuzz)
  {
    GLEXT_glUseProgramObjectARB(sh_indexed->hShader);
    active_shader = sh_indexed;
  }
}

void glsl_SetLightLevel(float lightlevel)
{
  GLEXT_glUniform1fARB(indexed_unifs.lightlevel_index, lightlevel);
}
