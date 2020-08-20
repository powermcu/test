/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *  Copyright (C) 2018-2020 - natinusala
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

#include "../gfx_widgets.h"
#include "../gfx_animation.h"
#include "../gfx_display.h"
#include "../../retroarch.h"

struct gfx_widget_libretro_message_state
{
   gfx_timer_t timer;
   char message[512];

   unsigned width;
   float alpha;
};

typedef struct gfx_widget_libretro_message_state gfx_widget_libretro_message_state_t;

static gfx_widget_libretro_message_state_t p_w_libretro_message_st = {
   0.0f,
   {'\0'},
   0,
   0.0f
};

static gfx_widget_libretro_message_state_t* gfx_widget_libretro_message_get_state(void)
{
   return &p_w_libretro_message_st;
}

static void gfx_widget_libretro_message_fadeout(void *userdata)
{
   gfx_animation_ctx_entry_t entry;
   gfx_widget_libretro_message_state_t* state = gfx_widget_libretro_message_get_state();
   uintptr_t        tag = (uintptr_t)&state->timer;

   /* Start fade out animation */
   entry.cb             = NULL;
   entry.duration       = MSG_QUEUE_ANIMATION_DURATION;
   entry.easing_enum    = EASING_OUT_QUAD;
   entry.subject        = &state->alpha;
   entry.tag            = tag;
   entry.target_value   = 0.0f;
   entry.userdata       = NULL;

   gfx_animation_push(&entry);
}

void gfx_widget_set_libretro_message(void *data,
      const char *msg, unsigned duration)
{
   gfx_timer_ctx_entry_t timer;
   gfx_widget_libretro_message_state_t* state = gfx_widget_libretro_message_get_state();
   uintptr_t tag                              = (uintptr_t)&state->timer;
   gfx_widget_font_data_t* font_regular       = gfx_widgets_get_font_regular(data);

   strlcpy(state->message, msg, sizeof(state->message));

   state->alpha = DEFAULT_BACKDROP;

   /* Kill and restart the timer / animation */
   gfx_timer_kill(&state->timer);
   gfx_animation_kill_by_tag(&tag);

   timer.cb       = gfx_widget_libretro_message_fadeout;
   timer.duration = duration;
   timer.userdata = NULL;

   gfx_timer_start(&state->timer, &timer);

   /* Compute text width */
   state->width = font_driver_get_message_width(font_regular->font, msg, (unsigned)strlen(msg), 1) + gfx_widgets_get_padding(data) * 2;
}

static void gfx_widget_libretro_message_frame(void *data, void *user_data)
{
   gfx_widget_libretro_message_state_t* state = 
      gfx_widget_libretro_message_get_state();

   if (state->alpha > 0.0f)
   {
      video_frame_info_t* video_info       = (video_frame_info_t*)data;
      void* userdata                       = video_info->userdata;
      unsigned video_width                 = video_info->width;
      unsigned video_height                = video_info->height;
      unsigned height                      = gfx_widgets_get_generic_message_height(user_data);
      float* backdrop_orign                = gfx_widgets_get_backdrop_orig();
      unsigned text_color                  = COLOR_TEXT_ALPHA(0xffffffff, (unsigned)(state->alpha*255.0f));
      gfx_widget_font_data_t* font_regular = gfx_widgets_get_font_regular(user_data);
      size_t msg_queue_size                = gfx_widgets_get_msg_queue_size(user_data);

      gfx_display_set_alpha(backdrop_orign, state->alpha);

      gfx_display_draw_quad(userdata,
            video_width, video_height,
            0, video_height - height,
            state->width, height,
            video_width, video_height,
            backdrop_orign);

      gfx_widgets_draw_text(font_regular, state->message,
            gfx_widgets_get_padding(user_data),
            video_height - height/2 + font_regular->line_centre_offset,
            video_width, video_height,
            text_color, TEXT_ALIGN_LEFT,
            false);

      /* If the message queue is active, must flush the
       * text here to avoid overlaps */
      if (msg_queue_size > 0)
         gfx_widgets_flush_text(video_width, video_height, font_regular);
   }
}

static void gfx_widget_libretro_message_free(void)
{
   gfx_widget_libretro_message_state_t* state = gfx_widget_libretro_message_get_state();
   uintptr_t tag                              = (uintptr_t) &state->timer;

   state->alpha = 0.0f;
   gfx_timer_kill(&state->timer);
   gfx_animation_kill_by_tag(&tag);
}

const gfx_widget_t gfx_widget_libretro_message = {
   NULL, /* init */
   gfx_widget_libretro_message_free,
   NULL, /* context_reset*/
   NULL, /* context_destroy */
   NULL, /* layout */
   NULL, /* iterate */
   gfx_widget_libretro_message_frame
};
