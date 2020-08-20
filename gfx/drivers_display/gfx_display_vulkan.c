/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../gfx_display.h"

#include "../font_driver.h"
#include "../../retroarch.h"
#include "../common/vulkan_common.h"

/* Will do Y-flip later, but try to make it similar to GL. */
static const float vk_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float vk_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float vk_colors[] = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

static void *gfx_display_vk_get_default_mvp(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return NULL;
   return &vk->mvp_no_rot;
}

static const float *gfx_display_vk_get_default_vertices(void)
{
   return &vk_vertexes[0];
}

static const float *gfx_display_vk_get_default_color(void)
{
   return &vk_colors[0];
}

static const float *gfx_display_vk_get_default_tex_coords(void)
{
   return &vk_tex_coords[0];
}

static unsigned to_display_pipeline(
      enum gfx_display_prim_type type, bool blend)
{
   return ((type == GFX_DISPLAY_PRIM_TRIANGLESTRIP) << 1) | (blend << 0);
}

#ifdef HAVE_SHADERPIPELINE
static unsigned to_menu_pipeline(
      enum gfx_display_prim_type type, unsigned pipeline)
{
   switch (pipeline)
   {
      case VIDEO_SHADER_MENU:
         return 4 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_2:
         return 6 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_3:
         return 8 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_4:
         return 10 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_5:
         return 12 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      default:
         return 0;
   }
}
#endif

static void gfx_display_vk_viewport(gfx_display_ctx_draw_t *draw,
      void *data)
{
   vk_t *vk                      = (vk_t*)data;

   if (!vk || !draw)
      return;

   vk->vk_vp.x        = draw->x;
   vk->vk_vp.y        = vk->context->swapchain_height - draw->y - draw->height;
   vk->vk_vp.width    = draw->width;
   vk->vk_vp.height   = draw->height;
   vk->vk_vp.minDepth = 0.0f;
   vk->vk_vp.maxDepth = 1.0f;
}

static void gfx_display_vk_draw_pipeline(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
#ifdef HAVE_SHADERPIPELINE
   static uint8_t ubo_scratch_data[768];
   static float t                   = 0.0f;
   float yflip                      = 0.0f;
   static struct video_coords blank_coords;
   float output_size[2];
   video_coord_array_t *ca          = NULL;
   vk_t *vk                         = (vk_t*)data;

   if (!vk || !draw)
      return;

   draw->x                          = 0;
   draw->y                          = 0;
   draw->matrix_data                = NULL;

   output_size[0]                   = (float)vk->context->swapchain_width;
   output_size[1]                   = (float)vk->context->swapchain_height;

   switch (draw->pipeline.id)
   {
      /* Ribbon */
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         ca = gfx_display_get_coords_array();
         draw->coords                     = (struct video_coords*)&ca->coords;
         draw->pipeline.backend_data      = ubo_scratch_data;
         draw->pipeline.backend_data_size = 2 * sizeof(float);

         /* Match UBO layout in shader. */
         yflip = 1.0f;
         memcpy(ubo_scratch_data, &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(float), &yflip, sizeof(yflip));
         break;

      /* Snow simple */
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
         draw->pipeline.backend_data      = ubo_scratch_data;
         draw->pipeline.backend_data_size = sizeof(math_matrix_4x4) 
            + 4 * sizeof(float);

         /* Match UBO layout in shader. */
         memcpy(ubo_scratch_data,
               gfx_display_vk_get_default_mvp(vk),
               sizeof(math_matrix_4x4));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4),
               output_size,
               sizeof(output_size));

         /* Shader uses FragCoord, need to fix up. */
         if (draw->pipeline.id == VIDEO_SHADER_MENU_5)
            yflip = -1.0f;
         else
            yflip = 1.0f;

         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4) 
               + 2 * sizeof(float), &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4) 
               + 3 * sizeof(float), &yflip, sizeof(yflip));
         draw->coords          = &blank_coords;
         blank_coords.vertices = 4;
         draw->prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
         break;
   }

   t += 0.01;
#endif
}

static void gfx_display_vk_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   unsigned i;
   struct vk_buffer_range range;
   struct vk_texture *texture    = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;
   struct vk_vertex *pv          = NULL;
   vk_t *vk                      = (vk_t*)data;

   if (!vk || !draw)
      return;

   texture            = (struct vk_texture*)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   if (!vertex)
      vertex          = gfx_display_vk_get_default_vertices();
   if (!tex_coord)
      tex_coord       = gfx_display_vk_get_default_tex_coords();
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = gfx_display_vk_get_default_tex_coords();
   if (!texture)
      texture         = &vk->display.blank_texture;
   if (!color)
      color           = gfx_display_vk_get_default_color();

   gfx_display_vk_viewport(draw, vk);

   vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;

   /* Bake interleaved VBO. Kinda ugly, we should probably try to move to
    * an interleaved model to begin with ... */
   if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
         draw->coords->vertices * sizeof(struct vk_vertex), &range))
      return;

   pv = (struct vk_vertex*)range.data;
   for (i = 0; i < draw->coords->vertices; i++, pv++)
   {
      pv->x       = *vertex++;
      /* Y-flip. Vulkan is top-left clip space */
      pv->y       = 1.0f - (*vertex++);
      pv->tex_x   = *tex_coord++;
      pv->tex_y   = *tex_coord++;
      pv->color.r = *color++;
      pv->color.g = *color++;
      pv->color.b = *color++;
      pv->color.a = *color++;
   }

   switch (draw->pipeline.id)
   {
#ifdef HAVE_SHADERPIPELINE
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      {
         struct vk_draw_triangles call;

         call.pipeline     = vk->display.pipelines[
               to_menu_pipeline(draw->prim_type, draw->pipeline.id)];
         call.texture      = NULL;
         call.sampler      = VK_NULL_HANDLE;
         call.uniform      = draw->pipeline.backend_data;
         call.uniform_size = draw->pipeline.backend_data_size;
         call.vbo          = &range;
         call.vertices     = draw->coords->vertices;

         vulkan_draw_triangles(vk, &call);
         break;
      }

         break;
#endif

      default:
      {
         struct vk_draw_triangles call;

         call.pipeline     = vk->display.pipelines[
               to_display_pipeline(draw->prim_type, vk->display.blend)];
         call.texture      = texture;
         call.sampler      = texture->mipmap ?
            vk->samplers.mipmap_linear :
            (texture->default_smooth ? vk->samplers.linear
             : vk->samplers.nearest);
         call.uniform      = draw->matrix_data
            ? draw->matrix_data : gfx_display_vk_get_default_mvp(vk);
         call.uniform_size = sizeof(math_matrix_4x4);
         call.vbo          = &range;
         call.vertices     = draw->coords->vertices;

         vulkan_draw_triangles(vk, &call);
         break;
      }
   }
}

static void gfx_display_vk_restore_clear_color(void)
{
}

static void gfx_display_vk_clear_color(
      gfx_display_ctx_clearcolor_t *clearcolor,
      void *data)
{
   VkClearRect rect;
   VkClearAttachment attachment;
   vk_t *vk                               = (vk_t*)data;
   if (!vk || !clearcolor)
      return;

   attachment.aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
   attachment.colorAttachment             = 0;
   attachment.clearValue.color.float32[0] = clearcolor->r;
   attachment.clearValue.color.float32[1] = clearcolor->g;
   attachment.clearValue.color.float32[2] = clearcolor->b;
   attachment.clearValue.color.float32[3] = clearcolor->a;

   rect.rect.offset.x                     = 0;
   rect.rect.offset.y                     = 0;
   rect.rect.extent.width                 = vk->context->swapchain_width;
   rect.rect.extent.height                = vk->context->swapchain_height;
   rect.baseArrayLayer                    = 0;
   rect.layerCount                        = 1;

   vkCmdClearAttachments(vk->cmd, 1, &attachment, 1, &rect);
}

static void gfx_display_vk_blend_begin(void *data)
{
   vk_t *vk = (vk_t*)data;

   if (vk)
      vk->display.blend = true;
}

static void gfx_display_vk_blend_end(void *data)
{
   vk_t *vk = (vk_t*)data;

   if (vk)
      vk->display.blend = false;
}

static bool gfx_display_vk_font_init_first(
      void **font_handle, void *video_data, const char *font_path,
      float menu_font_size, bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   *handle = font_driver_init_first(video_data,
         font_path, menu_font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_VULKAN_API);

   if (*handle)
      return true;

   return false;
}

static void gfx_display_vk_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   vk_t *vk                          = (vk_t*)data;

   vk->tracker.use_scissor           = true;
   vk->tracker.scissor.offset.x      = x;
   vk->tracker.scissor.offset.y      = y;
   vk->tracker.scissor.extent.width  = width;
   vk->tracker.scissor.extent.height = height;
   vk->tracker.dirty                |= VULKAN_DIRTY_DYNAMIC_BIT;
}

static void gfx_display_vk_scissor_end(void *data,
      unsigned video_width,
      unsigned video_height)
{
   vk_t *vk                 = (vk_t*)data;

   vk->tracker.use_scissor  = false;
   vk->tracker.dirty       |= VULKAN_DIRTY_DYNAMIC_BIT;
}

gfx_display_ctx_driver_t gfx_display_ctx_vulkan = {
   gfx_display_vk_draw,
   gfx_display_vk_draw_pipeline,
   gfx_display_vk_viewport,
   gfx_display_vk_blend_begin,
   gfx_display_vk_blend_end,
   gfx_display_vk_restore_clear_color,
   gfx_display_vk_clear_color,
   gfx_display_vk_get_default_mvp,
   gfx_display_vk_get_default_vertices,
   gfx_display_vk_get_default_tex_coords,
   gfx_display_vk_font_init_first,
   GFX_VIDEO_DRIVER_VULKAN,
   "vulkan",
   false,
   gfx_display_vk_scissor_begin,
   gfx_display_vk_scissor_end
};
