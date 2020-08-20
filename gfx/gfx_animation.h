/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Jean-André Santoni
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

#ifndef _GFX_ANIMATION_H
#define _GFX_ANIMATION_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "font_driver.h"

RETRO_BEGIN_DECLS

#define TICKER_SPACER_DEFAULT "   |   "

typedef void  (*tween_cb)  (void*);

typedef void (*update_time_cb) (float *ticker_pixel_increment,
      unsigned width, unsigned height);

enum gfx_animation_ctl_state
{
   MENU_ANIMATION_CTL_NONE = 0,
   MENU_ANIMATION_CTL_DEINIT,
   MENU_ANIMATION_CTL_CLEAR_ACTIVE,
   MENU_ANIMATION_CTL_SET_ACTIVE
};

enum gfx_animation_easing_type
{
   /* Linear */
   EASING_LINEAR    = 0,
   /* Quad */
   EASING_IN_QUAD,
   EASING_OUT_QUAD,
   EASING_IN_OUT_QUAD,
   EASING_OUT_IN_QUAD,
   /* Cubic */
   EASING_IN_CUBIC,
   EASING_OUT_CUBIC,
   EASING_IN_OUT_CUBIC,
   EASING_OUT_IN_CUBIC,
   /* Quart */
   EASING_IN_QUART,
   EASING_OUT_QUART,
   EASING_IN_OUT_QUART,
   EASING_OUT_IN_QUART,
   /* Quint */
   EASING_IN_QUINT,
   EASING_OUT_QUINT,
   EASING_IN_OUT_QUINT,
   EASING_OUT_IN_QUINT,
   /* Sine */
   EASING_IN_SINE,
   EASING_OUT_SINE,
   EASING_IN_OUT_SINE,
   EASING_OUT_IN_SINE,
   /* Expo */
   EASING_IN_EXPO,
   EASING_OUT_EXPO,
   EASING_IN_OUT_EXPO,
   EASING_OUT_IN_EXPO,
   /* Circ */
   EASING_IN_CIRC,
   EASING_OUT_CIRC,
   EASING_IN_OUT_CIRC,
   EASING_OUT_IN_CIRC,
   /* Bounce */
   EASING_IN_BOUNCE,
   EASING_OUT_BOUNCE,
   EASING_IN_OUT_BOUNCE,
   EASING_OUT_IN_BOUNCE,

   EASING_LAST
};

/* TODO:
 * Add a reverse loop ticker for languages
 * that read right to left */
enum gfx_animation_ticker_type
{
   TICKER_TYPE_BOUNCE = 0,
   TICKER_TYPE_LOOP,
   TICKER_TYPE_LAST
};

typedef struct gfx_animation_ctx_entry
{
   enum gfx_animation_easing_type easing_enum;
   uintptr_t tag;
   float duration;
   float target_value;
   float *subject;
   tween_cb cb;
   void *userdata;
} gfx_animation_ctx_entry_t;

typedef struct gfx_animation_ctx_ticker
{
   bool selected;
   size_t len;
   uint64_t idx;
   enum gfx_animation_ticker_type type_enum;
   char *s;
   const char *str;
   const char *spacer;
} gfx_animation_ctx_ticker_t;

typedef struct gfx_animation_ctx_ticker_smooth
{
   bool selected;
   font_data_t *font;
   float font_scale;
   unsigned glyph_width; /* Fallback if font == NULL */
   unsigned field_width;
   enum gfx_animation_ticker_type type_enum;
   uint64_t idx;
   const char *src_str;
   const char *spacer;
   char *dst_str;
   size_t dst_str_len;
   unsigned *dst_str_width; /* May be set to NULL (RGUI + XMB do not require this info) */
   unsigned *x_offset;
} gfx_animation_ctx_ticker_smooth_t;

typedef struct gfx_animation_ctx_line_ticker
{
   size_t line_len;
   size_t max_lines;
   uint64_t idx;
   enum gfx_animation_ticker_type type_enum;
   char *s;
   size_t len;
   const char *str;
} gfx_animation_ctx_line_ticker_t;

typedef struct gfx_animation_ctx_line_ticker_smooth
{
   bool fade_enabled;
   font_data_t *font;
   float font_scale;
   unsigned field_width;
   unsigned field_height;
   enum gfx_animation_ticker_type type_enum;
   uint64_t idx;
   const char *src_str;
   char *dst_str;
   size_t dst_str_len;
   float *y_offset;
   char *top_fade_str;
   size_t top_fade_str_len;
   float *top_fade_y_offset;
   float *top_fade_alpha;
   char *bottom_fade_str;
   size_t bottom_fade_str_len;
   float *bottom_fade_y_offset;
   float *bottom_fade_alpha;
} gfx_animation_ctx_line_ticker_smooth_t;

typedef float gfx_timer_t;

typedef struct gfx_timer_ctx_entry
{
   float duration;
   tween_cb cb;
   void *userdata;
} gfx_timer_ctx_entry_t;

typedef struct gfx_delayed_animation
{
   gfx_timer_t timer;
   gfx_animation_ctx_entry_t entry;
} gfx_delayed_animation_t;

void gfx_timer_start(gfx_timer_t *timer, gfx_timer_ctx_entry_t *timer_entry);

void gfx_timer_kill(gfx_timer_t *timer);

bool gfx_animation_update(
      retro_time_t current_time,
      bool timedate_enable,
      float ticker_speed,
      unsigned video_width,
      unsigned video_height);

bool gfx_animation_ticker(gfx_animation_ctx_ticker_t *ticker);

bool gfx_animation_ticker_smooth(gfx_animation_ctx_ticker_smooth_t *ticker);

bool gfx_animation_line_ticker(gfx_animation_ctx_line_ticker_t *line_ticker);

bool gfx_animation_line_ticker_smooth(gfx_animation_ctx_line_ticker_smooth_t *line_ticker);

float gfx_animation_get_delta_time(void);

bool gfx_animation_is_active(void);

bool gfx_animation_kill_by_tag(uintptr_t *tag);

bool gfx_animation_push(gfx_animation_ctx_entry_t *entry);

void gfx_animation_push_delayed(unsigned delay, gfx_animation_ctx_entry_t *entry);

bool gfx_animation_ctl(enum gfx_animation_ctl_state state, void *data);

uint64_t gfx_animation_get_ticker_idx(void);

uint64_t gfx_animation_get_ticker_slow_idx(void);

uint64_t gfx_animation_get_ticker_pixel_idx(void);

uint64_t gfx_animation_get_ticker_pixel_line_idx(void);

void gfx_animation_set_update_time_cb(update_time_cb cb);

void gfx_animation_unset_update_time_cb(void);

RETRO_END_DECLS

#endif
