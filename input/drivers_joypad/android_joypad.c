/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2013-2014 - Steven Crowe
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../config.def.h"

#include "../input_driver.h"
#include "../drivers_keyboard/keyboard_event_android.h"

static const char *android_joypad_name(unsigned pad)
{
   return input_config_get_device_name(pad);
}

static bool android_joypad_init(void *data) { return true; }

static int16_t android_joypad_button_state(
      struct android_app *android_app,
      uint8_t *buf,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      unsigned h = GET_HAT(joykey);
      if (h > 0)
         return 0;

      switch (hat_dir)
      {
         case HAT_LEFT_MASK:
            return (android_app->hat_state[port][0] == -1);
         case HAT_RIGHT_MASK:
            return (android_app->hat_state[port][0] == 1);
         case HAT_UP_MASK:
            return (android_app->hat_state[port][1] == -1);
         case HAT_DOWN_MASK:
            return (android_app->hat_state[port][1] == 1);
         default:
            break;
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < LAST_KEYCODE)
      return BIT_GET(buf, joykey);
   return 0;
}

static int16_t android_joypad_button(unsigned port, uint16_t joykey)
{
   struct android_app *android_app = (struct android_app*)g_android;
   uint8_t *buf                    = android_keyboard_state_get(port);

   if (port >= DEFAULT_MAX_PADS)
      return 0;

   return android_joypad_button_state(android_app, buf, port, joykey);
}

static int16_t android_joypad_axis_state(
      struct android_app *android_app,
      unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < MAX_AXIS)
   {
      int val = android_app->analog_state[port][AXIS_NEG_GET(joyaxis)];
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < MAX_AXIS)
   {
      int val = android_app->analog_state[port][AXIS_POS_GET(joyaxis)];
      if (val > 0)
         return val;
   }

   return 0;
}

static int16_t android_joypad_axis(unsigned port, uint32_t joyaxis)
{
   struct android_app *android_app = (struct android_app*)g_android;
   return android_joypad_axis_state(android_app, port, joyaxis);
}

static int16_t android_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   struct android_app *android_app      = (struct android_app*)g_android;
   uint8_t *buf                         = android_keyboard_state_get(port);

   if (port >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if ((uint16_t)joykey != NO_BTN 
            && android_joypad_button_state(
               android_app,
               buf,
               port, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(android_joypad_axis_state(
                  android_app, port, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void android_joypad_poll(void) { }

static bool android_joypad_query_pad(unsigned pad)
{
   return (pad < MAX_USERS);
}

static void android_joypad_destroy(void)
{
   unsigned i, j;
   struct android_app *android_app = (struct android_app*)g_android;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      for (j = 0; j < 2; j++)
         android_app->hat_state[i][j]    = 0;
      for (j = 0; j < MAX_AXIS; j++)
         android_app->analog_state[i][j] = 0;
   }
}

input_device_driver_t android_joypad = {
   android_joypad_init,
   android_joypad_query_pad,
   android_joypad_destroy,
   android_joypad_button,
   android_joypad_state,
   NULL,
   android_joypad_axis,
   android_joypad_poll,
   NULL,
   android_joypad_name,
   "android",
};
