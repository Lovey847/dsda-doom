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
 *   OpenGL 3.3 texture handling
 */

#include "gl3_main.h"
#include "gl3_texture.h"

#include "lprintf.h"
#include "i_system.h"
#include "w_wad.h"
#include "r_patch.h"
#include "m_argv.h"

#include "dsda/palette.h"

#include <stdio.h>

////////////////////////////
// Texture page packer

// Packing region, split into multiple smaller regions
typedef struct region_s {
  struct region_s *next;

  int x, y, width, height;
} region_t;

// Packing rectangle, what's stuffed into the regions
struct rect_s;

typedef void (*rect_renderFunc_t)(struct rect_s *r);

// Rect rendering functions
static void RenderPatch(struct rect_s *r);
static void RenderPatchRot(struct rect_s *r);
static void RenderTexture(struct rect_s *r);
static void RenderTextureRot(struct rect_s *r);
static void RenderFlat(struct rect_s *r);

typedef struct rect_s {
  gl3_img_t *img; // Image this rectangle refers to

  rect_renderFunc_t render; // Function to render rect into texture

  size_t page; // Texture page this rect resides in

  // The first rectangle of a texture page is always at 0, 0.
  // That means whenever a rectangle is at 0, 0, the rectangle
  // couldn't fit on the last page and a new page should be made
  int x, y, width, height;

  // Render data
  union {
    struct {
      int lump;
    } patch;

    struct {
      int lump;
    } flat;

    struct {
      int tex;
    } texture;
  } data;

  dboolean packed; // Has this rectangle been packed into a page yet?
} rect_t;

// Routines for adding various graphics to rectangle list

// Add patch graphic to rectangle list
static void AddP(rect_renderFunc_t render, rect_renderFunc_t renderRot,
                 int id, int width, int height, rect_t *out)
{
  // Check if rect should be flipped
  if (width > height) {
    out->width = height;
    out->height = width;
    out->render = renderRot;
  } else {
    out->width = width;
    out->height = height;
    out->render = render;
  }
}

// Add patch
static void AddPatch(rect_t *r, int plump) {
  const rpatch_t *p;

  r->data.patch.lump = plump;

  p = R_CachePatchNum(plump);

  // Store patch rotated by default, rotation makes it upright
  AddP(RenderPatch, RenderPatchRot, plump, p->height, p->width, r);
  R_UnlockPatchNum(plump);
}


// Add texture
static void AddTexture(rect_t *r, int tex) {
  const rpatch_t *p;

  r->data.texture.tex = tex;

  p = R_CacheTextureCompositePatchNum(tex);

  // Store patch rotated by default, rotation makes it upright but flipped
  AddP(RenderTexture, RenderTextureRot, tex, p->height, p->width, r);
  R_UnlockTextureCompositePatchNum(tex);
}

// Add flat
static void AddFlat(rect_t *r, int flump) {
  r->data.flat.lump = flump;
  r->render = RenderFlat;
  r->width = r->height = 64;
}

// Non-recursive rectangle quicksort routine
static void SortRects(rect_t *r, size_t rcnt) {
  rect_t tmp;
  rect_t pivotval;

  // quicksort stack
  struct {
    size_t l, r;
  } *stack, *curstack;
  size_t stacksize = 64;

  size_t left, right, pivot, less, greater;

  // Allocate stack
  stack = Z_Malloc(stacksize*sizeof(*stack), PU_STATIC, NULL);

  if (rcnt <= 1) return;

  stack[0].l = 0;
  stack[0].r = rcnt-1;
  for (curstack = stack; curstack >= stack;) {
    // Resize stack, if necessary
    if (curstack-stack == stacksize) {
      stack = Z_Realloc(stack, (stacksize += 64)*sizeof(*stack), PU_STATIC, NULL);
      curstack = stack+stacksize-64;
    }

    // Load current stack
    left = curstack->l;
    right = curstack->r;

    // Process iteration
    pivot = (left + right)/2;
    pivotval = r[pivot];

    // Partition
    less = left;
    greater = right;

    for (;;) {
      // I'm sorting biggest to smallest, so lesser items should be bigger
      // Find shorter or thinner rect
      while ((r[less].height > pivotval.height) ||
             ((r[less].height == pivotval.height) && (r[less].width > pivotval.width)))
      {
        ++less;
      }

      // Find taller or wider rect
      while ((r[greater].height < pivotval.height) ||
             ((r[greater].height == pivotval.height) && (r[greater].width < pivotval.width)))
      {
        --greater;
      }

      if (less >= greater) break;

      // If less is to the left of greater, swap them so the
      // taller rect is on the left and the shorter rect is
      // on the right
      tmp = r[less];
      r[less] = r[greater];
      r[greater] = tmp;

      ++less;
      --greater;
    }

    // Recurse
    if (greater+1 < right) {
      curstack->l = greater+1;
//    curstack->r = right;

      if (left < greater) {
        ++curstack;

        curstack->l = left;
        curstack->r = greater;
      }
    } else if (left < greater) {
//    curstack->l = left;
      curstack->r = greater;
    } else --curstack;
  }

  Z_Free(stack);
}

// Pack rectangles into pages, using regions
static void PackRects(rect_t *r, size_t rcnt) {
  region_t *regionbuf, *region, *freeregion, regionval;
  rect_t * const end = r+rcnt;
  rect_t *unpacked = r; // First unpacked rectangle
  size_t curpage = 0;

  regionbuf = Z_Malloc((sizeof(rect_t)*2)*rcnt, PU_STATIC, NULL);

  // Initialize first region
  regionbuf->next = NULL;
  regionbuf->x = 0;
  regionbuf->y = 0;
  regionbuf->width = gl3_GL_MAX_TEXTURE_SIZE;
  regionbuf->height = gl3_GL_MAX_TEXTURE_SIZE;

  // Set first free region
  freeregion = regionbuf+1;

  // Sort rectangle array, from tallest to shortest
  SortRects(r, rcnt);

  // Do this while there are unpacked rectangles
  // Loop through all rectangles
  do {
    r = unpacked;
    unpacked = NULL;

    // If we've reached the page limit, error out
    if (curpage >= GL3_MAXPAGES) I_Error("PackRects: Ran out of room!\n");

    for (; r != end; ++r) {
      // If rectangle is already packed, skip it
      if (r->packed) continue;

      // Find region for rectangle
      for (region = regionbuf; region; region = region->next) {
        // If there's a region to the right of this one, with
        // the same y coordinate, and the same height, merge them together
        if (region->next) {
          while (region->next &&
                 (region->next->y == region->y) &&
                 (region->next->height == region->height) &&
                 (region->next->x == region->x+region->width))
          {
            region->width += region->next->width;
            region->next = region->next->next;
          }
        }

        if ((r->width > region->width) ||
            (r->height > region->height)) continue;

        // Region found, split region into 2
        regionval = *region; // Save value of old region

        r->x = regionval.x;
        r->y = regionval.y;
        r->page = curpage;
        r->packed = true;

        // If this is the first rectangle of the row, split differently
        if (r->x == 0) {
          // Right region, prioritize this when searching for regions
          region->next = freeregion;
          region->x = regionval.x+r->width;
          region->y = regionval.y;
          region->width = regionval.width-r->width;
          region->height = r->height;

          // Bottom region
          freeregion->next = regionval.next;
          freeregion->x = regionval.x;
          freeregion->y = regionval.y+r->height;
          freeregion->width = regionval.width;
          freeregion->height = regionval.height-r->height;
        } else {
          // Bottom region, prioritize this when searching for regions
          region->next = freeregion;
          region->x = regionval.x;
          region->y = regionval.y+r->height;
          region->width = r->width;
          region->height = regionval.height-r->height;

          // Right region
          freeregion->next = regionval.next;
          freeregion->x = regionval.x+r->width;
          freeregion->y = regionval.y;
          freeregion->width = regionval.width-r->width;
          freeregion->height = regionval.height;
        }

        ++freeregion;

        // Found region for rectangle
        break;
      }

      if (!region) {
        // Can't pack rectangle into texture page, save it for the next page
        if (!unpacked) unpacked = r;
      }
    }

    // Setup new region for next rectangle
    regionbuf->next = NULL;
    regionbuf->x = 0;
    regionbuf->y = 0;
    regionbuf->width = gl3_GL_MAX_TEXTURE_SIZE;
    regionbuf->height = gl3_GL_MAX_TEXTURE_SIZE;

    // Increment current texture page
    ++curpage;
  } while (unpacked);

  // Free regions
  Z_Free(regionbuf);
}

//////////////////////////
// Texture page renderer

// Render patch into texture, used by render functions
// Also set up image
static void RenderP(const rpatch_t *p, rect_t *r, dboolean rot) {
  size_t x, post, y;
  byte *out;
  dsda_playpal_t *playpaldata;

  // Align width to a 4-byte boundary for glTexSubImage2D
  const int alignedwidth = (r->width+3)&~3;

  // p->pixels doesn't have transparency pixels, so we have to do this ourselves
  out = Z_Malloc(alignedwidth*r->height, PU_STATIC, NULL);
  playpaldata = dsda_PlayPalData();

  memset(out, playpaldata->transparent, alignedwidth*r->height);

  if (rot) {
    // Render rotated patch
    // Looks upright since patches are stored column by column,
    // and are usually rendered rotated since it's faster
    for (x = 0; x < r->width; ++x) {
      for (post = 0; post < p->columns[x].numPosts; ++post) {
        for (y = p->columns[x].posts[post].topdelta + p->columns[x].posts[post].length;
             y-- > p->columns[x].posts[post].topdelta;)
        {
          out[y*alignedwidth + x] = p->columns[x].pixels[y];
        }
      }
    }
  } else {
    for (x = 0; x < r->height; ++x) {
      for (post = 0; post < p->columns[x].numPosts; ++post) {
        memcpy(out + x*alignedwidth + p->columns[x].posts[post].topdelta,
               p->columns[x].pixels + p->columns[x].posts[post].topdelta,
               p->columns[x].posts[post].length);
      }
    }
  }

  GL3(glTexSubImage2D(GL_TEXTURE_2D, 0,
                      r->x, r->y, r->width, r->height,
                      GL_RED_INTEGER, GL_UNSIGNED_BYTE, out));

  Z_Free(out);

  // Set image properties
  r->img->page = r->page;

  if (rot) {
    r->img->tl.x = r->img->bl.x = r->x;
    r->img->tr.x = r->img->br.x = r->x+r->width;

    r->img->tl.y = r->img->tr.y = r->y;
    r->img->bl.y = r->img->br.y = r->y+r->height;
  } else {
    r->img->tl.x = r->img->tr.x = r->x;
    r->img->bl.x = r->img->br.x = r->x+r->width;

    r->img->tl.y = r->img->bl.y = r->y;
    r->img->tr.y = r->img->br.y = r->y+r->height;
  }

  r->img->leftoffset = p->leftoffset;
  r->img->topoffset = p->topoffset;

  r->img->width = p->width;
  r->img->height = p->height;
}

// Render patch into texture page
static void RenderPatch(struct rect_s *r) {
  const rpatch_t *p = R_CachePatchNum(r->data.patch.lump);
  RenderP(p, r, false);
  R_UnlockPatchNum(r->data.patch.lump);
}

// Render rotated patch into texture page
static void RenderPatchRot(struct rect_s *r) {
  const rpatch_t *p = R_CachePatchNum(r->data.patch.lump);
  RenderP(p, r, true);
  R_UnlockPatchNum(r->data.patch.lump);
}

// Render texture into texture page
static void RenderTexture(struct rect_s *r) {
  const rpatch_t *p = R_CacheTextureCompositePatchNum(r->data.texture.tex);
  RenderP(p, r, false);
  R_UnlockTextureCompositePatchNum(r->data.texture.tex);
}

// Render rotated texture into texture page
static void RenderTextureRot(struct rect_s *r) {
  const rpatch_t *p = R_CacheTextureCompositePatchNum(r->data.texture.tex);
  RenderP(p, r, true);
  R_UnlockTextureCompositePatchNum(r->data.texture.tex);
}

// Render flat into texture page
static void RenderFlat(struct rect_s *r) {
  const byte *f = W_CacheLumpNum(r->data.flat.lump);
  GL3(glTexSubImage2D(GL_TEXTURE_2D, 0,
                      r->x, r->y, 64, 64,
                      GL_RED_INTEGER, GL_UNSIGNED_BYTE, f));
  W_UnlockLumpNum(r->data.flat.lump);

  // Set image properties
  r->img->page = r->page;

  r->img->tl.x = r->img->bl.x = r->x;
  r->img->tr.x = r->img->br.x = r->x+r->width;

  r->img->tl.y = r->img->tr.y = r->y;
  r->img->bl.y = r->img->br.y = r->y+r->height;

  r->img->leftoffset = r->img->topoffset = 0;
  r->img->width = r->img->height = 64;
}

// Render all rectangles into texture pages
static void RenderRects(rect_t *rects, size_t rcnt) {
  rect_t *end = rects+rcnt;
  size_t curpage = 0;

  for (; rects != end; ++rects) {
    // Check if this is on a new page
    if (rects->page >= gl3_pagecount) {
      lprintf(LO_INFO, "RenderRects: Making page %d\n", (int)gl3_pagecount);
      curpage = gl3_pagecount;
      GL3(glActiveTexture(GL3_TEXTURE_PAGE0+curpage));

      // TODO: Save size of texture page so this uses less memory!
      GL3(glGenTextures(1, gl3_texpages+curpage));
      GL3(glBindTexture(GL_TEXTURE_2D, gl3_texpages[curpage]));

      // Set texture parameters
      GL3(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
      GL3(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
      GL3(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
      GL3(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

      GL3(glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI,
                       gl3_GL_MAX_TEXTURE_SIZE, gl3_GL_MAX_TEXTURE_SIZE,
                       0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL));
      if (gl3_errno != GL_NO_ERROR)
        I_Error("Couldn't allocate texture page!\n");

      ++gl3_pagecount;
    }

    // Check if this rect is on a different page
    if (rects->page != curpage) {
      curpage = rects->page;
      GL3(glActiveTexture(GL3_TEXTURE_PAGE0+curpage));
    }

    rects->render(rects);
  }
}

///////////////////////
// Texture handling

gl3_img_t *gl3_images;
size_t gl3_imagecount;

// Lump number to image LUT
gl3_img_t **gl3_lumpimg;

// Texture ID to image LUT
gl3_img_t **gl3_teximg;

GLuint gl3_paltex;
GLuint gl3_texpages[GL3_MAXPAGES];
size_t gl3_pagecount = 0;

static void gl3_InitPal(void) {
  int x, y, z;
  int width, height, depth;

  // playpal: PLAYPAL lump
  // colmap: COLORMAP lump
  // outpal: Output palette, in RGBA8 format
  //         Filled for each combination of PLAYPAL and COLORMAP
  const byte *playpal, *colmap;
  byte *outpal;

  int colmapnum;

  dsda_playpal_t *playpaldata = dsda_PlayPalData();

  // Set active texture
  GL3(glActiveTexture(GL3_TEXTURE_PALETTE));

  // Make palette texture
  GL3(glGenTextures(1, &gl3_paltex));

  // Number of playpals
  width = W_LumpLength(W_GetNumForName(playpaldata->lump_name))/768;

  // Number of colormaps
  colmapnum = W_GetNumForName("COLORMAP");
  height = W_LumpLength(colmapnum)/256;

  // Depth (index into colormap, always 256)
  depth = 256;

  GL3(glBindTexture(GL_TEXTURE_3D, gl3_paltex));

  // Set texture parameters
  GL3(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GL3(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  GL3(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL3(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  GL3(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));

  // Create texture
  GL3(glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8,
                   width, height, depth,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

  // If we failed to create the texture, error out
  if (gl3_errno != GL_NO_ERROR)
    I_Error("gl3_InitTextures: Cannot create palette texture!\n");

  // Fill palette texture, one palette at a time
  playpal = V_GetPlaypal();
  colmap = W_CacheLumpNum(colmapnum);
  outpal = (byte*)Z_Malloc(depth*4, PU_STATIC, NULL);

  for (x = 0; x < width; ++x) {
    for (y = 0; y < height; ++y) {
      for (z = 0; z < depth; ++z) {
        const size_t ind = 768*x + 3*colmap[256*y + z];

        outpal[z*4] = playpal[ind];
        outpal[z*4 + 1] = playpal[ind+1];
        outpal[z*4 + 2] = playpal[ind+2];
        outpal[z*4 + 3] = 255 * (z != playpaldata->transparent);
      }

      GL3(glTexSubImage3D(GL_TEXTURE_3D, 0,
                          x, y, 0,
                          1, 1, depth,
                          GL_RGBA, GL_UNSIGNED_BYTE, outpal));
    }
  }

  Z_Free(outpal);
  W_UnlockLumpNum(colmapnum);
}

static void gl3_InitPages(void) {
  // Load all patches, textures, flats, etc. into the texture pages
  // Unidentifiable patch list
  static const char * const patchlist[] = {
    // dsda-doom.wad
    "DIG033", "DIG034", "DIG035", "DIG036", "DIG037", "DIG038", "DIG039",
    "DIG040", "DIG041", "DIG042", "DIG043", "DIG044", "DIG045", "DIG046",
    "DIG047", "DIG048", "DIG049", "DIG050", "DIG051", "DIG052", "DIG053",
    "DIG054", "DIG055", "DIG056", "DIG057", "DIG058", "DIG059", "DIG060",
    "DIG061", "DIG062", "DIG063", "DIG064", "DIG065", "DIG066", "DIG067",
    "DIG068", "DIG069", "DIG070", "DIG071", "DIG072", "DIG073", "DIG074",
    "DIG075", "DIG076", "DIG077", "DIG078", "DIG079", "DIG080", "DIG081",
    "DIG082", "DIG083", "DIG084", "DIG085", "DIG086", "DIG087", "DIG088",
    "DIG089", "DIG090", "DIG091", "DIG092", "DIG093", "DIG094", "DIG095",
    "DIG096", "DIG097", "DIG098", "DIG099", "DIG100", "DIG101", "DIG102",
    "DIG103", "DIG104", "DIG105", "DIG106", "DIG107", "DIG108", "DIG109",
    "DIG110", "DIG111", "DIG112", "DIG113", "DIG114", "DIG115", "DIG116",
    "DIG117", "DIG118", "DIG119", "DIG120", "DIG121", "DIG122", "DIG123",
    "DIG124", "DIG125", "DIG126", "STBR123", "STBR124", "STBR125", "STBR126",
    "STBR127", "BOXUL", "BOXUC", "BOXUR", "BOXCL", "BOXCC", "BOXCR", "BOXLL",
    "BOXLC", "BOXLR", "STKEYS6", "STKEYS7", "STKEYS8", "STCFN096", "M_BUTT1",
    "M_BUTT2", "M_COLORS", "M_PALNO", "M_PALSEL", "M_VBOX", "CROSS1",
    "CROSS2", "CROSS3",

    // Ultimate DOOM
    "HELP1", "CREDIT", "VICTORY2", "TITLEPIC", "PFUB1", "PFUB2",
    "END0", "END1", "END2", "END3", "END4", "END5", "END6",
    "ENDPIC", "AMMNUM0", "AMMNUM1", "AMMNUM2", "AMMNUM3", "AMMNUM4",
    "AMMNUM5", "AMMNUM6", "AMMNUM7", "AMMNUM8", "AMMNUM9", "STBAR",
    "STGNUM0", "STGNUM1", "STGNUM2", "STGNUM3", "STGNUM4", "STGNUM5",
    "STGNUM6", "STGNUM7", "STGNUM8", "STGNUM9", "STTMINUS", "STTNUM0",
    "STTNUM1", "STTNUM2", "STTNUM3", "STTNUM4", "STTNUM5", "STTNUM6",
    "STTNUM7", "STTNUM8", "STTNUM9", "STTPRCNT", "STYSNUM0", "STYSNUM1",
    "STYSNUM2", "STYSNUM3", "STYSNUM4", "STYSNUM5", "STYSNUM6", "STYSNUM7",
    "STYSNUM8", "STYSNUM9", "STKEYS0", "STKEYS1", "STKEYS2", "STKEYS3",
    "STKEYS4", "STKEYS5", "STDISK", "STCDROM", "STARMS", "STCFN033",
    "STCFN034", "STCFN035", "STCFN036", "STCFN037", "STCFN038", "STCFN039",
    "STCFN040", "STCFN041", "STCFN042", "STCFN043", "STCFN044", "STCFN045",
    "STCFN046", "STCFN047", "STCFN048", "STCFN049", "STCFN050", "STCFN051",
    "STCFN052", "STCFN053", "STCFN054", "STCFN055", "STCFN056",
    "STCFN057", "STCFN058", "STCFN059", "STCFN060", "STCFN061", "STCFN062",
    "STCFN063", "STCFN064", "STCFN065", "STCFN066", "STCFN067", "STCFN068",
    "STCFN069", "STCFN070", "STCFN071", "STCFN072", "STCFN073", "STCFN074",
    "STCFN075", "STCFN076", "STCFN077", "STCFN078", "STCFN079", "STCFN080",
    "STCFN081", "STCFN082", "STCFN083", "STCFN084", "STCFN085", "STCFN086",
    "STCFN087", "STCFN088", "STCFN089", "STCFN090", "STCFN091", "STCFN092",
    "STCFN093", "STCFN094", "STCFN095", "STCFN121", "STFB1", "STFB0", "STFB2",
    "STFB3", "STPB1", "STPB0", "STPB2", "STPB3",
    "STFST01", "STFST00", "STFST02", "STFTL00", "STFTR00", "STFOUCH0", "STFEVL0", "STFKILL0",
    "STFST11", "STFST10", "STFST12", "STFTL10", "STFTR10", "STFOUCH1", "STFEVL1", "STFKILL1",
    "STFST21", "STFST20", "STFST22", "STFTL20", "STFTR20", "STFOUCH2", "STFEVL2", "STFKILL2",
    "STFST31", "STFST30", "STFST32", "STFTL30", "STFTR30", "STFOUCH3", "STFEVL3", "STFKILL3",
    "STFST41", "STFST40", "STFST42", "STFTL40", "STFTR40", "STFOUCH4", "STFEVL4", "STFKILL4",
    "STFGOD0", "STFDEAD0", "M_DOOM", "M_RDTHIS", "M_OPTION", "M_QUITG", "M_NGAME", "M_SKULL1",
    "M_SKULL2", "M_THERMO", "M_THERMR", "M_THERMM", "M_THERML", "M_ENDGAM", "M_PAUSE", "M_MESSG",
    "M_MSGON", "M_MSGOFF", "M_EPISOD", "M_EPI1", "M_EPI2", "M_EPI3", "M_HURT", "M_JKILL",
    "M_ROUGH", "M_SKILL", "M_NEWG", "M_ULTRA", "M_NMARE", "M_SVOL", "M_OPTTTL", "M_SAVEG",
    "M_LOADG", "M_DISP", "M_MSENS", "M_GDHIGH", "M_GDLOW", "M_DETAIL", "M_DISOPT", "M_SCRNSZ",
    "M_SGTTL", "M_LGTTL", "M_SFXVOL", "M_MUSVOL", "M_LSLEFT", "M_LSCNTR", "M_LSRIGHT",
    "BRDR_TL", "BRDR_T", "BRDR_TR", "BRDR_L", "BRDR_R", "BRDR_BL", "BRDR_B", "BRDR_BR",
    "M_EPI4", "WIMAP0", "WIA00900", "WIA00901", "WIA00902", "WIA00800", "WIA00801",
    "WIA00802", "WIA00700", "WIA00701", "WIA00702", "WIA00600", "WIA00601", "WIA00602",
    "WIA00500", "WIA00501", "WIA00502", "WIA00400", "WIA00401", "WIA00402", "WIA00300",
    "WIA00301", "WIA00302", "WIA00200", "WIA00201", "WIA00202", "WIA00100", "WIA00101",
    "WIA00102", "WIA00000", "WIA00001", "WIA00002", "WIURH0", "WIURH1", "WISPLAT",
    "WIMAP1", "WIA10000", "WIA10100", "WIA10200", "WIA10300", "WIA10400", "WIA10500",
    "WIA10600", "WIA10700", "WIA10701", "WIA10702", "WIMAP2", "WIA20000", "WIA20001",
    "WIA20002", "WIA20100", "WIA20101", "WIA20102", "WIA20200", "WIA20201", "WIA20202",
    "WIA20300", "WIA20301", "WIA20302", "WIA20400", "WIA20401", "WIA20402", "WIA20500",
    "WIA20501", "WIA20502", "INTERPIC", "WIOSTK", "WIOSTI", "WIF", "WIMSTT", "WIOSTS",
    "WIOSTF", "WITIME", "WIPAR", "WIMSTAR", "WIMINUS", "WIPCNT", "WINUM0", "WINUM1",
    "WINUM2", "WINUM3", "WINUM4", "WINUM5", "WINUM6", "WINUM7", "WINUM8", "WINUM9",
    "WICOLON", "WISUCKS", "WIFRGS", "WILV00", "WILV01", "WILV02", "WILV03", "WILV04",
    "WILV05", "WILV06", "WILV07", "WILV08", "WILV11", "WILV12", "WILV14",
    "WILV15", "WILV16", "WILV17", "WILV18", "WILV20", "WILV21", "WILV22", "WILV23",
    "WILV24", "WILV25", "WILV26", "WILV27", "WILV28", "WILV13", "WILV10", "WIP1",
    "WIP2", "WIP3", "WIP4", "WIBP1", "WIBP2", "WIBP3", "WIBP4", "WIKILRS", "WIVCTMS",
    "WISCRT2", "WIENTER", "WILV33", "WILV31", "WILV35", "WILV34", "WILV30", "WILV32",
    "WILV36", "WILV37", "WILV38",

    // DOOM II: Hell on Earth
    "HELP", "BOSSBACK", "CWILV00", "CWILV01", "CWILV02", "CWILV03", "CWILV04", "CWILV05",
    "CWILV06", "CWILV07", "CWILV08", "CWILV09", "CWILV10", "CWILV11", "CWILV12", "CWILV13",
    "CWILV14", "CWILV15", "CWILV16", "CWILV17", "CWILV18", "CWILV19", "CWILV20", "CWILV21",
    "CWILV22", "CWILV23", "CWILV24", "CWILV25", "CWILV26", "CWILV27", "CWILV28", "CWILV29",
    "CWILV30", "CWILV31",

    // TODO: Add more games!
  };

  gl3_img_t *img;
  rect_t *rectbuf, *rect;
  size_t i;
  int lump;
  int sstart, send;
  int fstart, fend;

  fstart = W_GetNumForName("F_START")+1;
  fend = W_GetNumForName("F_END");

  sstart = W_GetNumForName("S_START")+1;
  send = W_GetNumForName("S_END");

  gl3_imagecount =
    sizeof(patchlist)/sizeof(patchlist[0]) +
    send-sstart +
    numtextures +
    fend-fstart;

  img = gl3_images = Z_Malloc(sizeof(gl3_img_t)*gl3_imagecount, PU_STATIC, NULL);
  rect = rectbuf = Z_Malloc(sizeof(rect_t)*gl3_imagecount, PU_STATIC, NULL);

  // Allocate lump and texture id LUT
  gl3_lumpimg = Z_Malloc(sizeof(gl3_img_t**)*numlumps, PU_STATIC, NULL);
  gl3_teximg = Z_Malloc(sizeof(gl3_img_t**)*numtextures, PU_STATIC, NULL);

  // Make sure rectbuf is filled with 0's
  memset(rectbuf, 0, sizeof(rect_t)*gl3_imagecount);

  // Go through patch list, adding each one
  for (i = 0; i < sizeof(patchlist)/sizeof(patchlist[0]); ++i) {
    lump = W_CheckNumForName(patchlist[i]);
    if (lump < 0) {
      --gl3_imagecount;
      continue;
    }

    gl3_lumpimg[lump] = img;
    rect->img = img++;
    AddPatch(rect++, lump);
  }

  // Go through sprites, adding each one
  for (lump = sstart; lump < send; ++lump) {
    if (W_LumpLength(lump) == 0) {
      --gl3_imagecount;
      continue;
    }

    gl3_lumpimg[lump] = img;
    rect->img = img++;
    AddPatch(rect++, lump);
  }

  // Go through textures, adding each one
  for (lump = 0; lump < numtextures; ++lump) {
    gl3_teximg[lump] = img;
    rect->img = img++;
    AddTexture(rect++, lump);
  }

  // Go through flats, adding each one
  for (lump = fstart; lump < fend; ++lump) {
    if (W_LumpLength(lump) != 4096) {
      --gl3_imagecount;
      continue;
    }

    gl3_lumpimg[lump] = img;
    rect->img = img++;
    AddFlat(rect++, lump);
  }

  // Set default lumps, for lumps that don't get put in texture page
  // This should never happen, but I don't want it to be an error condition
  lump = W_CheckNumForName("-N0_TEX-");
  if (lump >= 0) {
    for (i = 0; i < numlumps; ++i) {
      if (!gl3_lumpimg[i])
        gl3_lumpimg[i] = gl3_lumpimg[lump];
    }
  }

  // All rectangles added, pack rectangles
  PackRects(rectbuf, gl3_imagecount);

  // Now render all rectangles into texture pages
  RenderRects(rectbuf, gl3_imagecount);

  Z_Free(rectbuf);

  // DEBUG: Log all images
  if (M_CheckParm("-gl3debug_writeimages")) {
    FILE *out = fopen("img.txt", "w");
    for (i = 0; i < gl3_imagecount; ++i)
      fprintf(out,
              "Image %d:\n"
              "  Texture page: %d\n"
              "  Bounds:\n"
              "    Top left: %hd %hd\n"
              "    Top right: %hd %hd\n"
              "    Bottom left: %hd %hd\n"
              "    Bottom right: %hd %hd\n"
              "  Offset: %d %d\n"
              "  Size: %d %d\n",

              (int)i,
              (int)gl3_images[i].page,
              gl3_images[i].tl.x, gl3_images[i].tl.y,
              gl3_images[i].tr.x, gl3_images[i].tr.y,
              gl3_images[i].bl.x, gl3_images[i].bl.y,
              gl3_images[i].br.x, gl3_images[i].br.y,
              gl3_images[i].leftoffset, gl3_images[i].topoffset,
              gl3_images[i].width, gl3_images[i].height);
    fclose(out);
  }

  // DEBUG: Go through patchlist and make sure nothing clashes
  if (M_CheckParm("-gl3debug_testpatchlist")) {
    size_t j;
    for (i = 0; i < sizeof(patchlist)/sizeof(patchlist[0]); ++i)
      for (j = i; j < sizeof(patchlist)/sizeof(patchlist[0]); ++j)
        if (!strcmp(patchlist[i], patchlist[j]) && (i != j))
          lprintf(LO_INFO, "%d and %d are %s!\n", (int)i, (int)j, patchlist[i]);
  }

  // DEBUG: Output texture page
  if (M_CheckParm("-gl3debug_writepages")) {
    for (i = 0; i < gl3_pagecount; ++i) {
      char name[] = "page0.data";
      FILE *outf;
      byte *out = Z_Malloc(gl3_GL_MAX_TEXTURE_SIZE*gl3_GL_MAX_TEXTURE_SIZE*3, PU_STATIC, NULL), *o;

      const byte *playpal = V_GetPlaypal();

      GL3(glActiveTexture(GL3_TEXTURE_PAGE0+i));
      GL3(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, out));

      for (o = out+gl3_GL_MAX_TEXTURE_SIZE*gl3_GL_MAX_TEXTURE_SIZE*3 - 3; o != out-3; o -= 3) {
        byte ind = *o;
        o[2] = playpal[2+ind*3];
        o[1] = playpal[1+ind*3];
        o[0] = playpal[0+ind*3];
      }

      name[4] = '0'+i;
      outf = fopen(name, "wb");
      if (outf) {
        fwrite(out, 1, gl3_GL_MAX_TEXTURE_SIZE*gl3_GL_MAX_TEXTURE_SIZE*3, outf);
        fclose(outf);
      } else lprintf(LO_INFO, "gl3_InitPages: Failed to create %s!\n", name);

      Z_Free(out);
    }
  }
}

void gl3_InitTextures(void) {
  // Initialize textures
  gl3_InitPal();
  gl3_InitPages();

  // TODO: When OpenGL 3.3 is fully implemented, we must _actually_ free up
  //       OpenGL resources when switching video modes, instead of doing it
  //       at exit, since VID_MODEGL and VID_MODEGL3 share resources!
  I_AtExit(gl3_DeleteTextures, true);
}

void gl3_DeleteTextures(void) {
  GL3(glDeleteTextures(1, &gl3_paltex));
  GL3(glDeleteTextures(gl3_pagecount, gl3_texpages));

  Z_Free(gl3_images);
  Z_Free(gl3_lumpimg);
  Z_Free(gl3_teximg);
  gl3_imagecount = gl3_pagecount = 0;
}
