/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdint.h>

#include "../../config.def.h"

#include "../input_driver.h"
#include "../../tasks/tasks_internal.h"

typedef struct
{
   XINPUT_STATE xstate;
   bool connected;
} xinput_joypad_state;

static xinput_joypad_state g_xinput_states[DEFAULT_MAX_PADS];

#ifdef _XBOX1
static HANDLE gamepads[DEFAULT_MAX_PADS];
#endif

static const char* const XBOX_CONTROLLER_NAMES[4] =
{
   "XInput Controller (User 1)",
   "XInput Controller (User 2)",
   "XInput Controller (User 3)",
   "XInput Controller (User 4)"
};

static const char *xdk_joypad_name(unsigned pad)
{
   return XBOX_CONTROLLER_NAMES[pad];
}

static void xdk_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
         xdk_joypad_name(autoconf_pad),
         NULL,
         xdk_joypad.ident,
         autoconf_pad,
         0,
         0);
}

static bool xdk_joypad_init(void *data)
{
#ifdef _XBOX1
   XInitDevices(0, NULL);
#else
   unsigned autoconf_pad;
   for (autoconf_pad = 0; autoconf_pad < MAX_USERS; autoconf_pad++)
      xdk_joypad_autodetect_add(autoconf_pad);
#endif

   (void)data;

   return true;
}

#ifndef _XBOX1
/* Buttons are provided by XInput as bits of a uint16.
 * Map from rarch button index (0..10) to a mask to bitwise-& the buttons against.
 * dpad is handled seperately. */
static const uint16_t button_index_to_bitmap_code[] =  {
   XINPUT_GAMEPAD_A,
   XINPUT_GAMEPAD_B,
   XINPUT_GAMEPAD_X,
   XINPUT_GAMEPAD_Y,
   XINPUT_GAMEPAD_LEFT_SHOULDER,
   XINPUT_GAMEPAD_RIGHT_SHOULDER,
   XINPUT_GAMEPAD_START,
   XINPUT_GAMEPAD_BACK,
   XINPUT_GAMEPAD_LEFT_THUMB,
   XINPUT_GAMEPAD_RIGHT_THUMB
};
#endif

static int16_t xdk_joypad_button_state(
      XINPUT_GAMEPAD *pad,
      uint16_t btn_word,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir  = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      switch (hat_dir)
      {
         case HAT_UP_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_UP);
         case HAT_DOWN_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_DOWN);
         case HAT_LEFT_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_LEFT);
         case HAT_RIGHT_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_RIGHT);
         default:
            break;
      }
      /* hat requested and no hat button down */
   }
   else
   {
#ifdef _XBOX1
      switch (joykey)
      {
         case RETRO_DEVICE_ID_JOYPAD_A:
            return (pad->bAnalogButtons[XINPUT_GAMEPAD_B] > XINPUT_GAMEPAD_MAX_CROSSTALK);
         case RETRO_DEVICE_ID_JOYPAD_B:
            return (pad->bAnalogButtons[XINPUT_GAMEPAD_A] > XINPUT_GAMEPAD_MAX_CROSSTALK);
         case RETRO_DEVICE_ID_JOYPAD_Y:
            return (pad->bAnalogButtons[XINPUT_GAMEPAD_X] > XINPUT_GAMEPAD_MAX_CROSSTALK);
         case RETRO_DEVICE_ID_JOYPAD_X:
            return (pad->bAnalogButtons[XINPUT_GAMEPAD_Y] > XINPUT_GAMEPAD_MAX_CROSSTALK)
         case RETRO_DEVICE_ID_JOYPAD_START:
               return (pad->wButtons & XINPUT_GAMEPAD_START);
         case RETRO_DEVICE_ID_JOYPAD_SELECT:
               return (pad->wButtons & XINPUT_GAMEPAD_BACK);
         case RETRO_DEVICE_ID_JOYPAD_L3:
               return (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
         case RETRO_DEVICE_ID_JOYPAD_R3:
               return (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
         case RETRO_DEVICE_ID_JOYPAD_L2:
               return (pad->bAnalogButtons[XINPUT_GAMEPAD_WHITE] > XINPUT_GAMEPAD_MAX_CROSSTALK);
         case RETRO_DEVICE_ID_JOYPAD_R2:
               return (pad->bAnalogButtons[XINPUT_GAMEPAD_BLACK] > XINPUT_GAMEPAD_MAX_CROSSTALK);
         case RETRO_DEVICE_ID_JOYPAD_L:
               return (pad->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] > XINPUT_GAMEPAD_MAX_CROSSTALK);
         case RETRO_DEVICE_ID_JOYPAD_R:
               return (pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] > XINPUT_GAMEPAD_MAX_CROSSTALK);
         default:
               break;
      }
#else
      if (joykey < 10)
         return (btn_word & button_index_to_bitmap_code[joykey]);
#endif
   }
   return 0;
}

static int16_t xdk_joypad_button(unsigned port, uint16_t joykey)
{
   uint16_t btn_word   = 0;
   XINPUT_GAMEPAD *pad = NULL;
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   pad                 = &(g_xinput_states[port].xstate.Gamepad);
   btn_word            = pad->wButtons;
   return xdk_joypad_button_state(pad, btn_word, port, joykey);
}

static int16_t xdk_joypad_axis_state(XINPUT_GAMEPAD *pad,
      unsigned port, uint32_t joyaxis)
{
   int val             = 0;
   int axis            = -1;
   bool is_neg         = false;
   bool is_pos         = false;

   if (AXIS_NEG_GET(joyaxis) <= 3)
   {
      axis   = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) <= 5)
   {
      axis   = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }
   else
      return 0;

   switch (axis)
   {
      case 0:
         val = pad->sThumbLX;
         break;
      case 1:
         val = pad->sThumbLY;
         break;
      case 2:
         val = pad->sThumbRX;
         break;
      case 3:
         val = pad->sThumbRY;
         break;
      case 4:
#ifdef _XBOX360
         val = pad->bLeftTrigger  * 32767 / 255;
#endif
         break; /* map 0..255 to 0..32767 */
      case 5:
#ifdef _XBOX360
         val = pad->bRightTrigger * 32767 / 255;
#endif
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   /* Clamp to avoid warnings */
   else if (val == -32768)
      return -32767;
   return val;
}

static int16_t xdk_joypad_axis(unsigned port, uint32_t joyaxis)
{
   XINPUT_GAMEPAD *pad = &(g_xinput_states[port].xstate.Gamepad);
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return xdk_joypad_axis_state(pad, port, joyaxis);
}

static int16_t xdk_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret         = 0;
   XINPUT_GAMEPAD *pad = NULL;
   uint16_t btn_word   = 0;

   if (port >= DEFAULT_MAX_PADS)
      return 0;

   pad                 = &(g_xinput_states[port].xstate.Gamepad);
   btn_word            = pad->wButtons;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && xdk_joypad_button_state(
               pad, btn_word, port, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(xdk_joypad_axis_state(pad, port, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void xdk_joypad_poll(void)
{
   unsigned port;
#if defined(_XBOX1)
   DWORD dwInsertions, dwRemovals;

#ifdef __cplusplus
   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD,
         reinterpret_cast<PDWORD>(&dwInsertions),
         reinterpret_cast<PDWORD>(&dwRemovals));
#else
   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD,
         (PDWORD)&dwInsertions,
         (PDWORD)&dwRemovals);
#endif
#endif

   for (port = 0; port < DEFAULT_MAX_PADS; port++)
   {
#if defined(_XBOX1)
      bool device_removed    = false;
      bool device_inserted   = false;

      /* handle inserted devices. */
      /* handle removed devices. */
      if (dwRemovals & (1 << port))
         device_removed = true;
      if (dwInsertions & (1 << port))
         device_inserted = true;

      if (device_removed)
      {
         /* if the controller was removed after
          * XGetDeviceChanges but before
          * XInputOpen, the device handle will be NULL. */
         if (gamepads[port])
            XInputClose(gamepads[port]);

         gamepads[port]  = 0;

         input_autoconfigure_disconnect(port, xdk_joypad.ident);
      }

      if (device_inserted)
      {
         XINPUT_POLLING_PARAMETERS m_pollingParameters;

         m_pollingParameters.fAutoPoll       = FALSE;
         m_pollingParameters.fInterruptOut   = TRUE;
         m_pollingParameters.bInputInterval  = 8;
         m_pollingParameters.bOutputInterval = 8;

         gamepads[port]                      = XInputOpen(
               XDEVICE_TYPE_GAMEPAD, port,
               XDEVICE_NO_SLOT, &m_pollingParameters);

         xdk_joypad_autodetect_add(port);
      }

      if (!gamepads[port])
         continue;

      /* if the controller is removed after
       * XGetDeviceChanges but before XInputOpen,
       * the device handle will be NULL. */
      if (XInputPoll(gamepads[port]) != ERROR_SUCCESS)
         continue;
#endif

      memset(&g_xinput_states[port], 0, sizeof(xinput_joypad_state));

      g_xinput_states[port].connected = !
      (XInputGetState(
#ifdef _XBOX1
         gamepads[port]
#else
         port
#endif
         , &g_xinput_states[port].xstate) == ERROR_DEVICE_NOT_CONNECTED);
   }
}

static bool xdk_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && g_xinput_states[pad].connected;
}

static void xdk_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      memset(&g_xinput_states[i], 0, sizeof(xinput_joypad_state));
#if defined(_XBOX1)
      if (gamepads[i])
         XInputClose(gamepads[i]);
      gamepads[i]  = 0;
#endif
   }
}

input_device_driver_t xdk_joypad = {
   xdk_joypad_init,
   xdk_joypad_query_pad,
   xdk_joypad_destroy,
   xdk_joypad_button,
   xdk_joypad_state,
   NULL,
   xdk_joypad_axis,
   xdk_joypad_poll,
   NULL,
   xdk_joypad_name,
   "xdk",
};
