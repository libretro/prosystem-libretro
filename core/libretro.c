#ifndef _MSC_VER
#include <stdbool.h>
#include <sched.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#pragma pack(1)
#endif

#include "libretro.h"
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

#ifdef _3DS
extern void* linearMemAlign(size_t size, size_t alignment);
extern void linearFree(void* mem);
#endif

#define VIDEO_BUFFER_SIZE (320 * 292 * 4)
static uint8_t *videoBuffer = NULL;
static uint8_t videoPixelBytes = 2;
static int videoWidth  = 320;
static int videoHeight = 240;
static uint32_t display_palette32[256] = {0};
static uint16_t display_palette16[256] = {0};
static uint8_t keyboard_data[17] = {0};

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
   environ_cb = cb;
   libretro_set_core_options(environ_cb);
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

static uint32_t sound_GetSampleLength(uint32_t length, uint32_t unit, uint32_t unitMax)
{
   uint32_t sampleLength = length / unitMax;
   uint32_t sampleRemain = length % unitMax;
   if(sampleRemain != 0 && sampleRemain >= unit)
      sampleLength++;
   return sampleLength;
}

#define SAMPLES_PER_SEC 48000

static void sound_Resample(const uint8_t* source, uint8_t* target, int length)
{
   int measurement = SAMPLES_PER_SEC;
   int sourceIndex = 0;
   int targetIndex = 0;

   int max = ((prosystem_frequency * prosystem_scanlines) << 1);

   while(targetIndex < length)
   {
      if(measurement >= max)
      {
         target[targetIndex++] = source[sourceIndex];
         measurement -= max;
      }
      else
      {
         sourceIndex++;
         measurement += SAMPLES_PER_SEC;
      }
   }
}

#define MAX_BUFFER_SIZE 8192

static void sound_Store(void)
{
   unsigned i;
   uint32_t length;
   uint8_t sample[MAX_BUFFER_SIZE];

   memset(sample, 0, MAX_BUFFER_SIZE);
   length = 48000 / prosystem_frequency;

   sound_Resample(tia_buffer, sample, length);

   /* Ballblazer, Commando, various homebrew and hacks */
   if(cartridge_pokey)
   {
      uint32_t index;
      uint8_t pokeySample[MAX_BUFFER_SIZE];
      memset(pokeySample, 0, MAX_BUFFER_SIZE);
      sound_Resample(pokey_buffer, pokeySample, length);
      for(index = 0; index < length; index++)
      {
         sample[index] += pokeySample[index];
         sample[index] = sample[index] / 2;
      }
   }

   /* Convert 8u to 16s */
   for(i = 0; i != length; i ++)
   {
      int16_t sample16 = (sample[i] << 8);
      int16_t frame[2] = {sample16, sample16};

      audio_cb((int16_t)frame[0], (int16_t)frame[1]);
#if 0
      audio_batch_cb(frame, 2);
#endif
   }
}

static void update_input(void)
{
   unsigned i,j;
   unsigned joypad_bits[2];

   // ----------------------------------------------------------------------------
   // SetInput
   // +----------+--------------+-------------------------------------------------
   // | Offset   | Controller   | Control
   // +----------+--------------+-------------------------------------------------
   // | 00       | Joystick 1   | Right
   // | 01       | Joystick 1   | Left
   // | 02       | Joystick 1   | Down
   // | 03       | Joystick 1   | Up
   // | 04       | Joystick 1   | Button 1
   // | 05       | Joystick 1   | Button 2
   // | 06       | Joystick 2   | Right
   // | 07       | Joystick 2   | Left
   // | 08       | Joystick 2   | Down
   // | 09       | Joystick 2   | Up
   // | 10       | Joystick 2   | Button 1
   // | 11       | Joystick 2   | Button 2
   // | 12       | Console      | Reset
   // | 13       | Console      | Select
   // | 14       | Console      | Pause
   // | 15       | Console      | Left Difficulty
   // | 16       | Console      | Right Difficulty
   // +----------+--------------+-------------------------------------------------

   input_poll_cb();

   for (j = 0; j < 2; j++)
   {
      if (libretro_supports_bitmasks)
         joypad_bits[j] = input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
      else
      {
         joypad_bits[j] = 0;
         for (i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R3+1); i++)
            joypad_bits[j] |= input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
      }
   }

   keyboard_data[0]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) ? 1 : 0;
   keyboard_data[1]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) ? 1 : 0;
   keyboard_data[2]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) ? 1 : 0;
   keyboard_data[3]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_UP) ? 1 : 0;
   keyboard_data[4]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
   keyboard_data[5]  = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;

   keyboard_data[6]  = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) ? 1 : 0;
   keyboard_data[7]  = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) ? 1 : 0;
   keyboard_data[8]  = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) ? 1 : 0;
   keyboard_data[9]  = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_UP) ? 1 : 0;
   keyboard_data[10] = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
   keyboard_data[11] = joypad_bits[1] & (1 << RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;

   keyboard_data[12] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_X) ? 1 : 0;
   keyboard_data[13] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) ? 1 : 0;
   keyboard_data[14] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_START) ? 1 : 0;
   keyboard_data[15] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_L) ? 1 : 0;
   keyboard_data[16] = joypad_bits[0] & (1 << RETRO_DEVICE_ID_JOYPAD_R) ? 1 : 0;
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
	info->valid_extensions = "a78|bin";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = (cartridge_region == REGION_NTSC) ? 60 : 50;
   info->timing.sample_rate    = 48000;
   info->geometry.base_width   = videoWidth;
   info->geometry.base_height  = videoHeight;
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
   return 32829;
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

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "1" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "2" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Console Reset" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Console Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Console Pause" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Left Difficulty" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Right Difficulty" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "1" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "2" },

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

   if (cartridge_Load((const uint8_t*)info->data, info->size))
   {
      char biospath[512];
      const char *system_directory_c = NULL;
#ifdef _WIN32
      char slash = '\\';
#else
      char slash = '/';
#endif

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

   return false;
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
   prosystem_Close();
   bios_Release();
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
   videoBuffer = (uint8_t*)linearMemAlign(VIDEO_BUFFER_SIZE, 128);
#else
   videoBuffer = (uint8_t*)malloc(VIDEO_BUFFER_SIZE);
#endif
}

void retro_deinit(void)
{
   libretro_supports_bitmasks = false;

   if (videoBuffer)
   {
#ifdef _3DS
      linearFree(videoBuffer);
#else
      free(videoBuffer);
#endif
      videoBuffer = NULL;
   }
}

void retro_reset(void)
{
    prosystem_Reset();
}

void retro_run(void)
{
   const uint8_t *buffer;
   uint32_t video_pitch;

   update_input();

   prosystem_ExecuteFrame(keyboard_data); // wants input

   videoWidth  = Rect_GetLength(&maria_visibleArea);
   videoHeight = Rect_GetHeight(&maria_visibleArea);
   buffer      = maria_surface + ((maria_visibleArea.top - maria_displayArea.top) * Rect_GetLength(&maria_visibleArea));
   video_pitch = 320;

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
