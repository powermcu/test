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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <windowsx.h>

#include <dinput.h>
#include <mmsystem.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../tasks/tasks_internal.h"
#include "../input_keymaps.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "dinput_joypad.h"

/* For DIJOYSTATE2 struct, rgbButtons will always have 128 elements */
#define ARRAY_SIZE_RGB_BUTTONS 128

#ifndef NUM_HATS
#define NUM_HATS 4
#endif

struct dinput_joypad_data
{
   LPDIRECTINPUTDEVICE8 joypad;
   DIJOYSTATE2 joy_state;
   char* joy_name;
   char* joy_friendly_name;
   int32_t vid;
   int32_t pid;
   LPDIRECTINPUTEFFECT rumble_iface[2];
   DIEFFECT rumble_props;
};

/* TODO/FIXME - static globals */
static struct dinput_joypad_data g_pads[MAX_USERS];
static unsigned g_joypad_cnt;
#ifdef HAVE_XINPUT
static unsigned g_last_xinput_pad_idx;
#endif

static const GUID common_xinput_guids[] = {
   {MAKELONG(0x28DE, 0x11FF),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}, /* Valve streaming pad */
   {MAKELONG(0x045E, 0x02A1),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}, /* Wired 360 pad */
   {MAKELONG(0x045E, 0x028E),0x0000,0x0000,{0x00,0x00,0x50,0x49,0x44,0x56,0x49,0x44}}  /* wireless 360 pad */
};

/* forward declarations */
void dinput_destroy_context(void);
bool dinput_init_context(void);

extern bool g_xinput_block_pads;
extern int g_xinput_pad_indexes[MAX_USERS];
extern LPDIRECTINPUT8 g_dinput_ctx;

bool dinput_joypad_get_vidpid_from_xinput_index(
      int32_t index, int32_t *vid,
      int32_t *pid, int32_t *dinput_index)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(g_xinput_pad_indexes); i++)
   {
      /* Found XInput pad? */
      if (index == g_xinput_pad_indexes[i])
      {
         if (vid)
            *vid = g_pads[i].vid;

         if (pid)
            *pid = g_pads[i].pid;

         if (dinput_index)
            *dinput_index = i;

         return true;
      }
   }

   return false;
}

static void dinput_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (g_pads[i].joypad)
      {
         if (g_pads[i].rumble_iface[0])
         {
            IDirectInputEffect_Stop(g_pads[i].rumble_iface[0]);
            IDirectInputEffect_Release(g_pads[i].rumble_iface[0]);
         }
         if (g_pads[i].rumble_iface[1])
         {
            IDirectInputEffect_Stop(g_pads[i].rumble_iface[1]);
            IDirectInputEffect_Release(g_pads[i].rumble_iface[1]);
         }

         IDirectInputDevice8_Unacquire(g_pads[i].joypad);
         IDirectInputDevice8_Release(g_pads[i].joypad);
      }

      free(g_pads[i].joy_name);
      g_pads[i].joy_name = NULL;
      free(g_pads[i].joy_friendly_name);
      g_pads[i].joy_friendly_name = NULL;

      input_config_clear_device_name(i);
   }

   g_joypad_cnt = 0;
   memset(g_pads, 0, sizeof(g_pads));

   /* Can be blocked by global Dinput context. */
   dinput_destroy_context();
}

static void dinput_create_rumble_effects(struct dinput_joypad_data *pad)
{
   DIENVELOPE dienv;
   DICONSTANTFORCE dicf;
   LONG direction                            = 0;
   DWORD axis                                = DIJOFS_X;

   dicf.lMagnitude                           = 0;

   dienv.dwSize                              = sizeof(DIENVELOPE);
   dienv.dwAttackLevel                       = 5000;
   dienv.dwAttackTime                        = 250000;
   dienv.dwFadeLevel                         = 0;
   dienv.dwFadeTime                          = 250000;

   pad->rumble_props.cAxes                   = 1;
   pad->rumble_props.dwTriggerButton         = DIEB_NOTRIGGER;
   pad->rumble_props.dwTriggerRepeatInterval = 0;
   pad->rumble_props.cbTypeSpecificParams    = sizeof(DICONSTANTFORCE);
   pad->rumble_props.dwDuration              = INFINITE;
   pad->rumble_props.dwFlags                 = DIEFF_CARTESIAN |
      DIEFF_OBJECTOFFSETS;
   pad->rumble_props.dwGain                  = 0;
   pad->rumble_props.dwSize                  = sizeof(DIEFFECT);
   pad->rumble_props.dwStartDelay            = 0;
   pad->rumble_props.lpEnvelope              = &dienv;
   pad->rumble_props.lpvTypeSpecificParams   = &dicf;
   pad->rumble_props.rgdwAxes                = &axis;
   pad->rumble_props.rglDirection            = &direction;

#ifdef __cplusplus
   if (IDirectInputDevice8_CreateEffect(pad->joypad, GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[0], NULL) != DI_OK)
      RARCH_WARN("[DINPUT]: Strong rumble unavailable.\n");
#else
   if (IDirectInputDevice8_CreateEffect(pad->joypad, &GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[0], NULL) != DI_OK)
      RARCH_WARN("[DINPUT]: Strong rumble unavailable.\n");
#endif

   axis = DIJOFS_Y;

#ifdef __cplusplus
   if (IDirectInputDevice8_CreateEffect(pad->joypad, GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[1], NULL) != DI_OK)
      RARCH_WARN("[DINPUT]: Weak rumble unavailable.\n");
#else
   if (IDirectInputDevice8_CreateEffect(pad->joypad, &GUID_ConstantForce,
         &pad->rumble_props, &pad->rumble_iface[1], NULL) != DI_OK)
      RARCH_WARN("[DINPUT]: Weak rumble unavailable.\n");
#endif
}

static BOOL CALLBACK enum_axes_cb(
      const DIDEVICEOBJECTINSTANCE *inst, void *p)
{
   DIPROPRANGE range;
   LPDIRECTINPUTDEVICE8 joypad = (LPDIRECTINPUTDEVICE8)p;

   range.diph.dwSize           = sizeof(DIPROPRANGE);
   range.diph.dwHeaderSize     = sizeof(DIPROPHEADER);
   range.diph.dwHow            = DIPH_BYID;
   range.diph.dwObj            = inst->dwType;
   range.lMin                  = -0x7fff;
   range.lMax                  = 0x7fff;

   IDirectInputDevice8_SetProperty(joypad, DIPROP_RANGE, &range.diph);

   return DIENUM_CONTINUE;
}

#ifdef HAVE_XINPUT
/* Based on SDL2's implementation. */
static bool guid_is_xinput_device(const GUID* product_guid)
{
   unsigned i, num_raw_devs = 0;
   PRAWINPUTDEVICELIST raw_devs = NULL;

   /* Check for well known XInput device GUIDs,
    * thereby removing the need for the IG_ check.
    * This lets us skip RAWINPUT for popular devices.
    *
    * Also, we need to do this for the Valve Streaming Gamepad
    * because it's virtualized and doesn't show up in the device list.  */

   for (i = 0; i < ARRAY_SIZE(common_xinput_guids); ++i)
   {
      if (string_is_equal_fast(product_guid,
               &common_xinput_guids[i], sizeof(GUID)))
         return true;
   }

   /* Go through RAWINPUT (WinXP and later) to find HID devices. */
   if (!raw_devs)
   {
      if ((GetRawInputDeviceList(NULL, &num_raw_devs,
                  sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) || (!num_raw_devs))
         return false;

      raw_devs = (PRAWINPUTDEVICELIST)
         malloc(sizeof(RAWINPUTDEVICELIST) * num_raw_devs);
      if (!raw_devs)
         return false;

      if (GetRawInputDeviceList(raw_devs, &num_raw_devs,
               sizeof(RAWINPUTDEVICELIST)) == (UINT)-1)
      {
         free(raw_devs);
         raw_devs = NULL;
         return false;
      }
   }

   for (i = 0; i < num_raw_devs; i++)
   {
      RID_DEVICE_INFO rdi;
      char *dev_name  = NULL;
      UINT rdi_size   = sizeof(rdi);
      UINT name_size  = 0;

      rdi.cbSize      = rdi_size;

      /* 
       * Step 1 -
       * Check if device type is HID
       * Step 2 -
       * Query size of name
       * Step 3 -
       * Allocate string holding ID of device
       * Step 4 -
       * query ID of device
       * Step 5 -
       * Check if the device ID contains "IG_".
       * If it does, then it's an XInput device
       * This information can not be found from DirectInput 
       */
      if (
               (raw_devs[i].dwType == RIM_TYPEHID)                    /* 1 */
            && (GetRawInputDeviceInfoA(raw_devs[i].hDevice,
                RIDI_DEVICEINFO, &rdi, &rdi_size) != ((UINT)-1))
            && (MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId)
             == ((LONG)product_guid->Data1))
            && (GetRawInputDeviceInfoA(raw_devs[i].hDevice,
                RIDI_DEVICENAME, NULL, &name_size) != ((UINT)-1))     /* 2 */
            && ((dev_name = (char*)malloc(name_size)) != NULL)        /* 3 */
            && (GetRawInputDeviceInfoA(raw_devs[i].hDevice,
                RIDI_DEVICENAME, dev_name, &name_size) != ((UINT)-1)) /* 4 */
            && (strstr(dev_name, "IG_"))                              /* 5 */
         )
      {
         free(dev_name);
         free(raw_devs);
         raw_devs = NULL;
         return true;
      }

      if (dev_name)
         free(dev_name);
   }

   free(raw_devs);
   raw_devs = NULL;
   return false;
}
#endif

static const char *dinput_joypad_name(unsigned port)
{
   if (port < MAX_USERS)
      return g_pads[port].joy_name;

   return NULL;
}

static BOOL CALLBACK enum_joypad_cb(const DIDEVICEINSTANCE *inst, void *p)
{
#ifdef HAVE_XINPUT
   bool is_xinput_pad;
#endif
   LPDIRECTINPUTDEVICE8 *pad = NULL;

   (void)p;

   if (g_joypad_cnt == MAX_USERS)
      return DIENUM_STOP;

   pad = &g_pads[g_joypad_cnt].joypad;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(
               g_dinput_ctx, inst->guidInstance, pad, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(
               g_dinput_ctx, &inst->guidInstance, pad, NULL)))
#endif
      return DIENUM_CONTINUE;

   g_pads[g_joypad_cnt].joy_name          = strdup((const char*)inst->tszProductName);
   g_pads[g_joypad_cnt].joy_friendly_name = strdup((const char*)inst->tszInstanceName);

   /* there may be more useful info in the GUID so leave this here for a while */
#if 0
   printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
   inst->guidProduct.Data1,
   inst->guidProduct.Data2,
   inst->guidProduct.Data3,
   inst->guidProduct.Data4[0],
   inst->guidProduct.Data4[1],
   inst->guidProduct.Data4[2],
   inst->guidProduct.Data4[3],
   inst->guidProduct.Data4[4],
   inst->guidProduct.Data4[5],
   inst->guidProduct.Data4[6],
   inst->guidProduct.Data4[7]);
#endif

   g_pads[g_joypad_cnt].vid = inst->guidProduct.Data1 % 0x10000;
   g_pads[g_joypad_cnt].pid = inst->guidProduct.Data1 / 0x10000;

#ifdef HAVE_XINPUT
   is_xinput_pad = g_xinput_block_pads
      && guid_is_xinput_device(&inst->guidProduct);

   if (is_xinput_pad)
   {
      if (g_last_xinput_pad_idx < 4)
         g_xinput_pad_indexes[g_joypad_cnt] = g_last_xinput_pad_idx++;
      goto enum_iteration_done;
   }
#endif

   /* Set data format to simple joystick */
   IDirectInputDevice8_SetDataFormat(*pad, &c_dfDIJoystick2);
   IDirectInputDevice8_SetCooperativeLevel(*pad,
         (HWND)video_driver_window_get(),
         DISCL_EXCLUSIVE | DISCL_BACKGROUND);

   IDirectInputDevice8_EnumObjects(*pad, enum_axes_cb,
         *pad, DIDFT_ABSAXIS);

   dinput_create_rumble_effects(&g_pads[g_joypad_cnt]);

#ifdef HAVE_XINPUT
   if (!is_xinput_pad)
#endif
   {
      input_autoconfigure_connect(
            g_pads[g_joypad_cnt].joy_name,
            g_pads[g_joypad_cnt].joy_friendly_name,
            dinput_joypad.ident,
            g_joypad_cnt,
            g_pads[g_joypad_cnt].vid,
            g_pads[g_joypad_cnt].pid);
   }

#ifdef HAVE_XINPUT
enum_iteration_done:
#endif
   g_joypad_cnt++;
   return DIENUM_CONTINUE;
}

static bool dinput_joypad_init(void *data)
{
   unsigned i;

   (void)data;

   if (!dinput_init_context())
      return false;

#ifdef HAVE_XINPUT
   g_last_xinput_pad_idx = 0;
#endif

   for (i = 0; i < MAX_USERS; ++i)
   {
      g_xinput_pad_indexes[i]     = -1;
      g_pads[i].joy_name          = NULL;
      g_pads[i].joy_friendly_name = NULL;
   }

   IDirectInput8_EnumDevices(g_dinput_ctx, DI8DEVCLASS_GAMECTRL,
         enum_joypad_cb, NULL, DIEDFL_ATTACHEDONLY);
   return true;
}

static int16_t dinput_joypad_button_state(
      const struct dinput_joypad_data *pad,
      uint16_t joykey)
{
   unsigned hat_dir                  = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      unsigned h = GET_HAT(joykey);
      if (h < NUM_HATS)
      {
         unsigned pov = pad->joy_state.rgdwPOV[h];
         switch (hat_dir)
         {
            case HAT_UP_MASK:
               {
                  static const unsigned check1 = (JOY_POVRIGHT/2);
                  static const unsigned check2 = (JOY_POVLEFT+JOY_POVRIGHT/2);
                  return (
                        (pov == JOY_POVFORWARD) ||
                        (pov == check1)         ||
                        (pov == check2)
                        );
               }
            case HAT_RIGHT_MASK:
               {
                  static const unsigned check1 = (JOY_POVRIGHT/2);
                  static const unsigned check2 = (JOY_POVRIGHT+JOY_POVRIGHT/2);
                  return (
                        (pov == JOY_POVRIGHT) ||
                        (pov == check1)       ||
                        (pov == check2)
                        );
               }
            case HAT_DOWN_MASK:
               {
                  static const unsigned check1 = (JOY_POVRIGHT+JOY_POVRIGHT/2);
                  static const unsigned check2 = (JOY_POVBACKWARD+JOY_POVRIGHT/2);
                  return 
                     (
                      (pov == JOY_POVBACKWARD) ||
                      (pov == check1)          ||
                      (pov == check2)
                     );
               }
            case HAT_LEFT_MASK:
               {
                  static const unsigned check1 = (JOY_POVBACKWARD+JOY_POVRIGHT/2);
                  static const unsigned check2 = (JOY_POVLEFT+JOY_POVRIGHT/2);

                  return
                     (
                      (pov == JOY_POVLEFT) || 
                      (pov == check1)      || 
                      (pov == check2)
                     );
               }
            default:
               break;
         }
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < ARRAY_SIZE_RGB_BUTTONS)
      if (pad->joy_state.rgbButtons[joykey])
         return 1;
   return 0;
}

static int16_t dinput_joypad_axis_state(
      const struct dinput_joypad_data *pad,
      uint32_t joyaxis)
{
   int val                              = 0;
   int axis                             = -1;
   bool is_neg                          = false;
   bool is_pos                          = false;

   if (AXIS_NEG_GET(joyaxis) <= 7)
   {
      axis   = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) <= 7)
   {
      axis   = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }
   else
      return 0;

   switch (axis)
   {
      case 0:
         val = pad->joy_state.lX;
         break;
      case 1:
         val = pad->joy_state.lY;
         break;
      case 2:
         val = pad->joy_state.lZ;
         break;
      case 3:
         val = pad->joy_state.lRx;
         break;
      case 4:
         val = pad->joy_state.lRy;
         break;
      case 5:
         val = pad->joy_state.lRz;
         break;
      case 6:
      case 7:
         val = pad->joy_state.rglSlider[axis - 6];
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   return val;
}

static int16_t dinput_joypad_button(unsigned port, uint16_t joykey)
{
   const struct dinput_joypad_data *pad = &g_pads[port];
   if (!pad || !pad->joypad)
      return 0;
   return dinput_joypad_button_state(pad, joykey);
}

static int16_t dinput_joypad_axis(unsigned port, uint32_t joyaxis)
{
   const struct dinput_joypad_data *pad = &g_pads[port];
   if (!pad || !pad->joypad)
      return 0;
   return dinput_joypad_axis_state(pad, joyaxis);
}

static int16_t dinput_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   const struct dinput_joypad_data *pad = &g_pads[port];

   if (!pad || !pad->joypad)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && dinput_joypad_button_state(
               pad, (uint16_t)joykey))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(dinput_joypad_axis_state(pad, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void dinput_joypad_poll(void)
{
   unsigned i;
   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;
      HRESULT ret;
      struct dinput_joypad_data *pad  = &g_pads[i];
      bool                    polled  = g_xinput_pad_indexes[i] < 0;

      if (!pad || !pad->joypad || !polled)
         continue;

      pad->joy_state.lX               = 0;
      pad->joy_state.lY               = 0;
      pad->joy_state.lRx              = 0;
      pad->joy_state.lRy              = 0;
      pad->joy_state.lRz              = 0;
      pad->joy_state.rglSlider[0]     = 0;
      pad->joy_state.rglSlider[1]     = 0;
      pad->joy_state.rgdwPOV[0]       = 0;
      pad->joy_state.rgdwPOV[1]       = 0;
      pad->joy_state.rgdwPOV[2]       = 0;
      pad->joy_state.rgdwPOV[3]       = 0;
      for (j = 0; j < 128; j++)
         pad->joy_state.rgbButtons[j] = 0;

      pad->joy_state.lVX              = 0;
      pad->joy_state.lVY              = 0;
      pad->joy_state.lVZ              = 0;
      pad->joy_state.lVRx             = 0;
      pad->joy_state.lVRy             = 0;
      pad->joy_state.lVRz             = 0;
      pad->joy_state.rglVSlider[0]    = 0;
      pad->joy_state.rglVSlider[1]    = 0;
      pad->joy_state.lAX              = 0;
      pad->joy_state.lAY              = 0;
      pad->joy_state.lAZ              = 0;
      pad->joy_state.lARx             = 0;
      pad->joy_state.lARy             = 0;
      pad->joy_state.lARz             = 0;
      pad->joy_state.rglASlider[0]    = 0;
      pad->joy_state.rglASlider[1]    = 0;
      pad->joy_state.lFX              = 0;
      pad->joy_state.lFY              = 0;
      pad->joy_state.lFZ              = 0;
      pad->joy_state.lFRx             = 0;
      pad->joy_state.lFRy             = 0;
      pad->joy_state.lFRz             = 0;
      pad->joy_state.rglFSlider[0]    = 0;
      pad->joy_state.rglFSlider[1]    = 0;

      /* If this fails, something *really* bad must have happened. */
      if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         if (
                  FAILED(IDirectInputDevice8_Acquire(pad->joypad))
               || FAILED(IDirectInputDevice8_Poll(pad->joypad))
            )
            continue;

      ret = IDirectInputDevice8_GetDeviceState(pad->joypad,
            sizeof(DIJOYSTATE2), &pad->joy_state);

      if (ret == DIERR_INPUTLOST || ret == DIERR_NOTACQUIRED)
         input_autoconfigure_disconnect(i, g_pads[i].joy_friendly_name);
   }
}

static bool dinput_joypad_query_pad(unsigned port)
{
   return port < MAX_USERS && g_pads[port].joypad;
}

bool dinput_joypad_set_rumble(unsigned port,
      enum retro_rumble_effect type, uint16_t strength)
{
   int i = type == RETRO_RUMBLE_STRONG ? 1 : 0;

   if (port >= g_joypad_cnt || !g_pads[port].rumble_iface[i])
      return false;

   if (strength)
   {
      g_pads[port].rumble_props.dwGain =
            (DWORD)((double)strength / 65535.0 * (double)DI_FFNOMINALMAX);
      IDirectInputEffect_SetParameters(g_pads[port].rumble_iface[i],
            &g_pads[port].rumble_props, DIEP_GAIN | DIEP_START);
   }
   else
      IDirectInputEffect_Stop(g_pads[port].rumble_iface[i]);

   return true;
}

input_device_driver_t dinput_joypad = {
   dinput_joypad_init,
   dinput_joypad_query_pad,
   dinput_joypad_destroy,
   dinput_joypad_button,
   dinput_joypad_state,
   NULL,
   dinput_joypad_axis,
   dinput_joypad_poll,
   dinput_joypad_set_rumble,
   dinput_joypad_name,
   "dinput",
};
