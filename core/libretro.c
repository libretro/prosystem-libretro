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

#include "Bios.h"
#include "Cartridge.h"
#include "Database.h"
#include "Maria.h"
#include "Palette.h"
#include "Pokey.h"
#include "Region.h"
#include "ProSystem.h"
#include "Tia.h"

static uint32_t videoBuffer[320*292*4];
static int videoWidth  = 320;
static int videoHeight = 240;
static uint32_t display_palette32[256] = {0};
static uint8_t keyboard_data[17] = {0};

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;

void retro_set_environment(retro_environment_t cb) { environ_cb = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

static void display_ResetPalette32(void)
{
   for(uint32_t index = 0; index < 256; index++)
   {
      uint32_t r = palette_data[(index * 3) + 0] << 16;
      uint32_t g = palette_data[(index * 3) + 1] << 8;
      uint32_t b = palette_data[(index * 3) + 2];
      display_palette32[index] = r | g | b;
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
      //audio_batch_cb(frame, 2);
   }
}

static void update_input(void)
{
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

   keyboard_data[0]  = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   keyboard_data[1]  = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
   keyboard_data[2]  = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
   keyboard_data[3]  = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
   keyboard_data[4]  = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
   keyboard_data[5]  = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);

   keyboard_data[6]  = input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   keyboard_data[7]  = input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
   keyboard_data[8]  = input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
   keyboard_data[9]  = input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
   keyboard_data[10] = input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
   keyboard_data[11] = input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);

   keyboard_data[12] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
   keyboard_data[13] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT);
   keyboard_data[14] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
   keyboard_data[15] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L);
   keyboard_data[16] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R);
}

/************************************
 * libretro implementation
 ************************************/

static struct retro_system_av_info g_av_info;

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
	info->library_name = "ProSystem";
	info->library_version = "1.3e";
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
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "[ProSystem]: XRGB8888 is not supported.\n");
      return false;
   }

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
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "1" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "2" },

      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   memset(keyboard_data, 0, sizeof(keyboard_data));

   // Difficulty switches: Left position = (B)eginner, Right position = (A)dvanced
   // Left difficulty switch defaults to left position, "(B)eginner"
   keyboard_data[15] = 1;

   // Right difficulty switch defaults to right position, "(A)dvanced", which fixes Tower Toppler
   keyboard_data[16] = 0;

   const char *system_directory_c = NULL;

   if (cartridge_Load((const uint8_t*)info->data, info->size))
   {
      char biospath[512];
      environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_directory_c);

      // BIOS is optional
      sprintf(biospath, "%s/%s", system_directory_c, "7800 BIOS (U).rom");
      if (bios_Load(biospath))
         bios_enabled = true;

      database_Load(cartridge_digest);
      prosystem_Reset();

      display_ResetPalette32();

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
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
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
}

void retro_deinit(void)
{
}

void retro_reset(void)
{
    prosystem_Reset();
}

void retro_run(void)
{
   uint32_t x, y;
   const uint8_t *buffer;
   uint32_t *surface, pitch;

   update_input();

   prosystem_ExecuteFrame(keyboard_data); // wants input

   videoWidth  = Rect_GetLength(&maria_visibleArea);
   videoHeight = Rect_GetHeight(&maria_visibleArea);
   buffer      = maria_surface + ((maria_visibleArea.top - maria_displayArea.top) * Rect_GetLength(&maria_visibleArea));
   surface     = (uint32_t*)videoBuffer;
   pitch       = 320;

   for(y = 0; y < videoHeight; y++)
   {
      for(x = 0; x < videoWidth; x += 4)
      {
         surface[x + 0] = display_palette32[buffer[x + 0]];
         surface[x + 1] = display_palette32[buffer[x + 1]];
         surface[x + 2] = display_palette32[buffer[x + 2]];
         surface[x + 3] = display_palette32[buffer[x + 3]];
      }
      surface += pitch;
      buffer  += videoWidth;
   }

   video_cb(videoBuffer, videoWidth, videoHeight, videoWidth << 2);

   sound_Store();
}
