#ifndef _MSC_VER
#include <stdbool.h>
#include <sched.h>
#endif
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <streams/file_stream.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#pragma pack(1)
#endif

#include <libretro.h>
#include "libretro_core_options.h"

#include "Bios.h"
#include "Cartridge.h"
#include "Database.h"
#include "Maria.h"
#include "Palette.h"
#include "Pokey.h"
#include "Region.h"
#include "ProSystem.h"
#include "Tia.h"
#include "Memory.h"
#include "BupChip.h"

#ifdef _3DS
extern void* linearMemAlign(size_t size, size_t alignment);
extern void linearFree(void* mem);
#endif

#define VIDEO_BUFFER_SIZE (320 * 292 * 4)
static uint8_t *videoBuffer            = NULL;
static uint8_t videoPixelBytes         = 2;
static int videoWidth                  = 320;
static int videoHeight                 = 240;
static uint32_t display_palette32[256] = {0};
static uint16_t display_palette16[256] = {0};
static uint8_t keyboard_data[17]       = {0};

static bool persistent_data            = false;

#define GAMEPAD_ANALOG_THRESHOLD 0x4000
static bool gamepad_dual_stick_hack    = false;

/* Required buffer size is exactly TIA_BUFFER_SIZE,
 * but round up to nearest multiple of 128 for
 * peace of mind... */
#define AUDIO_SAMPLE_BUFFER_SIZE ((TIA_BUFFER_SIZE + 0x7F) & ~0x7F)
static uint8_t *pokeyMixBuffer         = NULL;
static int16_t *audioOutBuffer         = NULL;

/* Low pass audio filter */
static bool low_pass_enabled           = false;
static int32_t low_pass_range          = 0;
static int32_t low_pass_prev           = 0; /* Previous sample */

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;

static bool libretro_supports_bitmasks = false;

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   static const struct retro_system_content_info_override content_overrides[] = {
      {
         "a78|bin|cdf", /* extensions */
         false,         /* need_fullpath */
         true           /* persistent_data */
      },
      { NULL, false, false }
   };

   environ_cb = cb;
   libretro_set_core_options(environ_cb);
   /* Request a persistent content data buffer */
   environ_cb(RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE,
         (void*)content_overrides);

   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface                      = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      filestream_vfs_init(&vfs_iface_info);
}

#define BLIT_VIDEO_BUFFER(typename_t, src, palette, width, height, pitch, dst) \
   {                                                                           \
      typename_t *surface = (typename_t*)dst;                                  \
      uint32_t x, y;                                                           \
                                                                               \
      for(y = 0; y < height; y++)                                              \
      {                                                                        \
         typename_t *surface_ptr = surface;                                    \
         const uint8_t *src_ptr  = src;                                        \
                                                                               \
         for(x = 0; x < width; x++)                                            \
            *(surface_ptr++) = *(palette + *(src_ptr++));                      \
                                                                               \
         surface += pitch;                                                     \
         src     += width;                                                     \
      }                                                                        \
   }

static void display_ResetPalette(void)
{
   unsigned index;

   for(index = 0; index < 256; index++)
   {
      uint32_t r = palette_data[(index * 3) + 0] << 16;
      uint32_t g = palette_data[(index * 3) + 1] << 8;
      uint32_t b = palette_data[(index * 3) + 2];
      display_palette32[index] = r | g | b;
      display_palette16[index] = ((r & 0xF80000) >> 8) |
                                 ((g & 0x00F800) >> 5) |
                                 ((b & 0x0000F8) >> 3);
   }
}

static short sound_Lerp(short a, short b, float t) {
   return (short)floorf((float)a + (float)(b - a) * t + 0.5f);
}

static void sound_ResampleBupChip(const short* source, short* target, int length)
{
   int targetIndex;
   uint32_t bupchipBufferSize = CORETONE_BUFFER_SAMPLES * 4;

   for(targetIndex = 0; targetIndex < length; targetIndex++)
   {
      float t;
      int channel;
      float sourceIndex = (float)targetIndex / (float)length * (float)bupchipBufferSize;
      uint32_t sourceLo = (uint32_t)floorf(sourceIndex), sourceHi = (uint32_t)ceilf(sourceIndex);
      if(sourceHi >= bupchipBufferSize)
         sourceHi = bupchipBufferSize;
      t = sourceIndex - (float)sourceLo;

      for (channel = 0; channel < 2; channel++)
      {
         int sample = sound_Lerp(source[sourceLo * 2 + channel], source[sourceHi * 2 + channel], t);
         sample += target[targetIndex * 2 + channel];
         if(sample > INT16_MAX)
            sample = INT16_MAX;
         else if(sample < INT16_MIN)
            sample = INT16_MIN;
         target[targetIndex * 2 + channel] = sample;
      }
   }
}

static void sound_Store(void)
{
   uint8_t *tia_samples_buf = tia_buffer;
   int16_t *audio_out_buf   = audioOutBuffer;
   size_t i, j;

   /* Mix in sound generated by POKEY chip
    * (Ballblazer, Commando, various homebrew and hacks) */
   if(cartridge_pokey)
   {
      uint8_t *pokey_samples_buf = pokey_buffer;
      uint8_t *pokey_mix_buf     = pokeyMixBuffer;

      /* Copy samples to pokeyMixBuffer */
      for(j = 0; j < tia_size; j++)
         *(pokey_mix_buf++) = (*(tia_samples_buf++) + *(pokey_samples_buf++)) >> 1;

      /* pokeyMixBuffer 'replaces' tia_buffer */
      tia_samples_buf = pokeyMixBuffer;
   }

   /* Convert 8u mono to 16s stereo, applying
    * low pass filter if enabled */
   if (low_pass_enabled)
   {
      /* Restore previous sample */
      int32_t low_pass = low_pass_prev;

      /* Single-pole low-pass filter (6 dB/octave) */
      int32_t factor_a = low_pass_range;
      int32_t factor_b = 0x10000 - factor_a;

      for(i = 0; i < tia_size; i++)
      {
         int16_t sample_16 = (int16_t)(*(tia_samples_buf++) << 8);

         /* Apply low-pass filter */
         low_pass = (low_pass * factor_a) + (sample_16 * factor_b);

         /* 16.16 fixed point */
         low_pass >>= 16;

         /* Update sound buffer */
         *(audio_out_buf++) = (int16_t)low_pass;
         *(audio_out_buf++) = (int16_t)low_pass;
      }

      /* Save last sample for next frame */
      low_pass_prev = low_pass;
   }
   else
   {
      for(i = 0; i < tia_size; i++)
      {
         int16_t sample_16 = (int16_t)(*(tia_samples_buf++) << 8);

         *(audio_out_buf++) = sample_16;
         *(audio_out_buf++) = sample_16;
      }
   }

   /* Mix in sound generated by BupChip
    * ("Rikki & Vikki") */
   if(cartridge_bupchip)
      sound_ResampleBupChip(bupchip_buffer, audioOutBuffer, tia_size);

   audio_batch_cb(audioOutBuffer, tia_size);
}

static void update_input(void)
{
   unsigned i,j;
   unsigned joypad_bits[2];
   unsigned j2_override_right = 0;
   unsigned j2_override_left  = 0;
   unsigned j2_override_down  = 0;
   unsigned j2_override_up    = 0;

    
   /*
    * ----------------------------------------------------------------------------
    * SetInput
    * +----------+--------------+-------------------------------------------------
    * | Offset   | Controller   | Control
    * +----------+--------------+-------------------------------------------------
    * | 00       | Joystick 1   | Right
    * | 01       | Joystick 1   | Left
    * | 02       | Joystick 1   | Down
    * | 03       | Joystick 1   | Up
    * | 04       | Joystick 1   | Button 1
    * | 05       | Joystick 1   | Button 2
    * | 06       | Joystick 2   | Right
    * | 07       | Joystick 2   | Left
    * | 08       | Joystick 2   | Down
    * | 09       | Joystick 2   | Up
    * | 10       | Joystick 2   | Button 1
    * | 11       | Joystick 2   | Button 2
    * | 12       | Console      | Reset
    * | 13       | Console      | Select
    * | 14       | Console      | Pause
    * | 15       | Console      | Left Difficulty
    * | 16       | Console      | Right Difficulty
    * +----------+--------------+-------------------------------------------------
    */

   input_poll_cb();

   if (libretro_supports_bitmasks)
   {
      for (j = 0; j < 2; j++)
         joypad_bits[j] = input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
   }
   else
   {
      for (j = 0; j < 2; j++)
      {
         joypad_bits[j] = 0;
         for (i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R3+1); i++)
            joypad_bits[j] |= input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
      }
   }

   /* If dual stick controller hack is enabled,
    * fetch overrides for player 2's joystick
    * right/left/down/up values */
   if (gamepad_dual_stick_hack)
   {
      int analog_x = input_state_cb(0, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
      int analog_y = input_state_cb(0, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

      if (analog_x >= GAMEPAD_ANALOG_THRESHOLD)
         j2_override_right = 1;
      else if (analog_x <= -GAMEPAD_ANALOG_THRESHOLD)
         j2_override_left  = 1;

      if (analog_y >= GAMEPAD_ANALOG_THRESHOLD)
         j2_override_down  = 1;
      else if (analog_y <= -GAMEPAD_ANALOG_THRESHOLD)
         j2_override_up    = 1;
   }

   keyboard_data[0]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)  ? 1 : 0;
   keyboard_data[1]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)   ? 1 : 0;
   keyboard_data[2]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)   ? 1 : 0;
   keyboard_data[3]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_UP)     ? 1 : 0;
   keyboard_data[4]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_B)      ? 1 : 0;
   keyboard_data[5]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_A)      ? 1 : 0;

   keyboard_data[6]  = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT)  ? 1 : j2_override_right;
   keyboard_data[7]  = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)   ? 1 : j2_override_left;
   keyboard_data[8]  = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)   ? 1 : j2_override_down;
   keyboard_data[9]  = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_UP)     ? 1 : j2_override_up;
   keyboard_data[10] = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_B)      ? 1 : 0;
   keyboard_data[11] = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_A)      ? 1 : 0;

   keyboard_data[12] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_X)      ? 1 : 0;
   keyboard_data[13] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) ? 1 : 0;
   keyboard_data[14] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_START)  ? 1 : 0;
   keyboard_data[15] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_L)      ? 1 : 0;
   keyboard_data[16] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_R)      ? 1 : 0;
}

void check_variables(bool first_run)
{
   struct retro_variable var = {0};

   /* Only read colour depth option on first run */
   if (first_run)
   {
      var.key   = "prosystem_color_depth";
      var.value = NULL;

      /* Set 16bpp by default */
      videoPixelBytes = 2;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
         if (strcmp(var.value, "24bit") == 0)
            videoPixelBytes = 4;
   }

   /* Read low pass audio filter settings */
   var.key   = "prosystem_low_pass_filter";
   var.value = NULL;

   low_pass_enabled = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      if (strcmp(var.value, "enabled") == 0)
         low_pass_enabled = true;

   var.key   = "prosystem_low_pass_range";
   var.value = NULL;

   low_pass_range = (60 * 0x10000) / 100;

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		low_pass_range = (strtol(var.value, NULL, 10) * 0x10000) / 100;

   /* Read dual stick controller setting */
   var.key   = "prosystem_gamepad_dual_stick_hack";
   var.value = NULL;

   gamepad_dual_stick_hack = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      if (strcmp(var.value, "enabled") == 0)
         gamepad_dual_stick_hack = true;
}

/************************************
 * libretro implementation
 ************************************/

static struct retro_system_av_info g_av_info;

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
	info->library_name = "ProSystem";
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
	info->library_version = "1.3e" GIT_VERSION;
	info->need_fullpath = false;
	info->valid_extensions = "a78|bin|cdf";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = (cartridge_region == REGION_NTSC) ? 60 : 50;
   info->timing.sample_rate    = (prosystem_frequency * prosystem_scanlines) << 1; /* 2 samples per scanline */
   info->geometry.base_width   = videoWidth;
   info->geometry.base_height  = (cartridge_region == REGION_NTSC) ? 223 : 272;
   info->geometry.max_width    = 320;
   info->geometry.max_height   = 292;
   info->geometry.aspect_ratio = 4.0 / 3.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

size_t retro_serialize_size(void) 
{ 
   return 49221;
}

bool retro_serialize(void *data, size_t size)
{ 
   return prosystem_Save((char*)data, false);
}

bool retro_unserialize(const void *data, size_t size)
{
   return prosystem_Load((const char*)data);
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt;
   const struct retro_game_info_ext *info_ext = NULL;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "1" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "Console Reset" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Console Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Console Pause" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "Left Difficulty" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Right Difficulty" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "(Dual Stick) P2 X-Axis" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "(Dual Stick) P2 Y-Axis" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "1" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "2" },

      { 0 },
   };

   if (!info)
      return false;

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   /* Set color depth */
   check_variables(true);

   if (videoPixelBytes == 4)
   {
      fmt = RETRO_PIXEL_FORMAT_XRGB8888;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      {
         if (log_cb)
            log_cb(RETRO_LOG_INFO, "[ProSystem]: XRGB8888 is not supported - trying RGB565...\n");

         /* Fallback to RETRO_PIXEL_FORMAT_RGB565 */
         videoPixelBytes = 2;
      }
   }

   if (videoPixelBytes == 2)
   {
      fmt = RETRO_PIXEL_FORMAT_RGB565;
      if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      {
         if (log_cb)
            log_cb(RETRO_LOG_INFO, "[ProSystem]: RGB565 is not supported.\n");
         return false;
      }
   }

   memset(keyboard_data, 0, sizeof(keyboard_data));

   /* Difficulty switches: 
    * Left position = (B)eginner, Right position = (A)dvanced
    * Left difficulty switch defaults to left position, "(B)eginner"
    */
   keyboard_data[15] = 1;
   /* Right difficulty switch defaults to right position,
    * "(A)dvanced", which fixes Tower Toppler
    */
   keyboard_data[16] = 0;

   if (environ_cb(RETRO_ENVIRONMENT_GET_GAME_INFO_EXT, &info_ext) &&
       info_ext->persistent_data)
      persistent_data = true;

#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif

   if (info->size >= 10 && memcmp(info->data, "ProSystem", 9) == 0)
   {
      /* CDF file. */
      char* lastSlash = strrchr(info->path, slash);
      size_t baseSize = lastSlash == NULL ? strlen(info->path) : lastSlash - info->path;
      char* workingDir = malloc(baseSize + 1);
      memcpy(workingDir, info->path, baseSize);
      workingDir[baseSize] = '\0';

      bool ok = cartridge_LoadFromCDF(info->data, info->size, workingDir);

      free(workingDir);

      if (!ok)
         return false;
   }
   else if (!cartridge_Load(persistent_data,
            (const uint8_t*)info->data, info->size))
   {
      return false;
   }

   char biospath[512];
   const char *system_directory_c = NULL;

   environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_directory_c);

   /* BIOS is optional */
   sprintf(biospath, "%s%c%s", system_directory_c, slash, "7800 BIOS (U).rom");
   if (bios_Load(biospath))
      bios_enabled = true;

   database_Load(cartridge_digest);
   prosystem_Reset();

   display_ResetPalette();

   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   (void)game_type;
   (void)info;
   (void)num_info;
   return false;
}

void retro_unload_game(void) 
{
   prosystem_Close(persistent_data);
   bios_Release();
   persistent_data = false;
}

unsigned retro_get_region(void)
{
    return cartridge_region == REGION_NTSC ? RETRO_REGION_NTSC : RETRO_REGION_PAL;
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void *retro_get_memory_data(unsigned id)
{
    if ( id == RETRO_MEMORY_SYSTEM_RAM )
        return memory_ram;
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    if ( id == RETRO_MEMORY_SYSTEM_RAM )
        return MEMORY_SIZE;
    return 0;
}

void retro_init(void)
{
   struct retro_log_callback log;
   unsigned level = 5;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;

#ifdef _3DS
   videoBuffer = (uint8_t*)linearMemAlign(VIDEO_BUFFER_SIZE * sizeof(uint8_t), 128);
#else
   videoBuffer = (uint8_t*)malloc(VIDEO_BUFFER_SIZE * sizeof(uint8_t));
#endif

   pokeyMixBuffer = (uint8_t*)malloc(AUDIO_SAMPLE_BUFFER_SIZE * sizeof(uint8_t));
   /* Samples are mono, output buffer is stereo
    * (AUDIO_SAMPLE_BUFFER_SIZE * 2) */
   audioOutBuffer = (int16_t*)malloc((AUDIO_SAMPLE_BUFFER_SIZE << 1) * sizeof(int16_t));
}

void retro_deinit(void)
{
   libretro_supports_bitmasks = false;
   gamepad_dual_stick_hack    = false;
   low_pass_enabled           = false;
   low_pass_prev              = 0;

   if (videoBuffer)
   {
#ifdef _3DS
      linearFree(videoBuffer);
#else
      free(videoBuffer);
#endif
      videoBuffer = NULL;
   }

   if (pokeyMixBuffer)
   {
      free(pokeyMixBuffer);
      pokeyMixBuffer = NULL;
   }

   if (audioOutBuffer)
   {
      free(audioOutBuffer);
      audioOutBuffer = NULL;
   }
}

void retro_reset(void)
{
    prosystem_Reset();
}

void retro_run(void)
{
   const uint8_t *buffer = NULL;
   uint32_t video_pitch  = 320;
   bool options_updated  = false;

   /* Core options */
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &options_updated) && options_updated)
      check_variables(false);

   update_input();

   prosystem_ExecuteFrame(keyboard_data); /* wants input */

   videoWidth  = Rect_GetLength(&maria_visibleArea);
   videoHeight = Rect_GetHeight(&maria_visibleArea);
   buffer      = maria_surface + ((maria_visibleArea.top - maria_displayArea.top) * Rect_GetLength(&maria_visibleArea));

   if (videoPixelBytes == 2)
   {
      BLIT_VIDEO_BUFFER(uint16_t, buffer, display_palette16, videoWidth, videoHeight, video_pitch, videoBuffer);
   }
   else
   {
      BLIT_VIDEO_BUFFER(uint32_t, buffer, display_palette32, videoWidth, videoHeight, video_pitch, videoBuffer);
   }

   video_cb(videoBuffer, videoWidth, videoHeight, videoWidth * videoPixelBytes);

   sound_Store();
}
