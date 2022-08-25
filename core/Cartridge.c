// ----------------------------------------------------------------------------
//   ___  ___  ___  ___       ___  ____  ___  _  _
//  /__/ /__/ /  / /__  /__/ /__    /   /_   / |/ /
// /    / \  /__/ ___/ ___/ ___/   /   /__  /    /  emulator
//
// ----------------------------------------------------------------------------
// Copyright 2005 Greg Stanton
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
// ----------------------------------------------------------------------------
// Cartridge.cpp
// ----------------------------------------------------------------------------
#include "Cartridge.h"
#include "Equates.h"
#include "Memory.h"
#include "Hash.h"
#include "Pokey.h"
#include "BupChip.h"
#include <streams/file_stream.h>
#include <stdlib.h>
#include <string.h>
#define CARTRIDGE_SOURCE "Cartridge.cpp"

char cartridge_digest[33];
uint8_t cartridge_type;
uint8_t cartridge_region;
bool cartridge_pokey;
uint8_t cartridge_controller[2];
uint8_t cartridge_bank;
uint32_t cartridge_flags;
bool cartridge_bupchip;

// SOUPER-specific stuff, used for "Rikki & Vikki"
uint8_t cartridge_souper_chr_bank[2];
uint8_t cartridge_souper_mode;
uint8_t cartridge_souper_ram_page_bank[2];

uint8_t* cartridge_buffer = NULL;
static uint32_t cartridge_size = 0;

// ----------------------------------------------------------------------------
// HasHeader
// ----------------------------------------------------------------------------
static bool cartridge_HasHeader(const uint8_t* header)
{
   unsigned index;
   const char HEADER_ID[ ] = {"ATARI7800"};

   for(index = 0; index < 9; index++)
   {
      if(HEADER_ID[index] != header[index + 1])
         return false;
   }
   return true;
}

// ----------------------------------------------------------------------------
// Header for CC2 hack
// ----------------------------------------------------------------------------
static bool cartridge_CC2(const uint8_t* header)
{
   unsigned index;
   const char HEADER_ID[ ] = {">>"};

   for(index = 0; index < 2; index++)
   {
      if(HEADER_ID[index] != header[index+1])
         return false;
   }
   return true;
}

// ----------------------------------------------------------------------------
// GetBankOffset
// ----------------------------------------------------------------------------
static uint32_t cartridge_GetBankOffset(uint8_t bank)
{
   if (
         (
          cartridge_type == CARTRIDGE_TYPE_SUPERCART || 
          cartridge_type == CARTRIDGE_TYPE_SUPERCART_ROM || 
          cartridge_type == CARTRIDGE_TYPE_SUPERCART_RAM) && cartridge_size <= 65536
      )
   {
      // for some of these carts, there are only 4 banks. in this case we ignore bit 3
      // previously, games of this type had to be doubled. The first 4 banks needed to be duplicated at the end of the ROM
      return (bank & 3) * 16384;
   }

   return bank * 16384;
}

// ----------------------------------------------------------------------------
// WriteBank
// ----------------------------------------------------------------------------
static void cartridge_WriteBank(uint16_t address, uint8_t bank)
{
  uint32_t offset = cartridge_GetBankOffset(bank);

  if(offset < cartridge_size)
  {
    memory_WriteROM(address, 16384, cartridge_buffer + offset);
    cartridge_bank = bank;
  }
}
// ----------------------------------------------------------------------------
// SOUPER StoreChrBank
// ----------------------------------------------------------------------------
static void cartridge_souper_StoreChrBank(uint8_t page, uint8_t bank) {
  if(page < 2)
    cartridge_souper_chr_bank[page] = bank;
}

// ----------------------------------------------------------------------------
// SOUPER SetMode
// ----------------------------------------------------------------------------
static void cartridge_souper_SetMode(uint8_t data) {
  cartridge_souper_mode = data;
}

// ----------------------------------------------------------------------------
// SOUPER SetVideoPageBank
// ----------------------------------------------------------------------------
static void cartridge_souper_SetRamPageBank(uint8_t which, uint8_t data) {
  if(which < 2)
    cartridge_souper_ram_page_bank[which] = data & 7;
}


// ----------------------------------------------------------------------------
// ReadHeader
// ----------------------------------------------------------------------------
static void cartridge_ReadHeader(const uint8_t* header)
{
   cartridge_size  = header[49] << 24;
   cartridge_size |= header[50] << 16;
   cartridge_size |= header[51] << 8;
   cartridge_size |= header[52];

   if(header[53] == 0)
   {
      if(cartridge_size > 131072)
         cartridge_type = CARTRIDGE_TYPE_SUPERCART_LARGE;
      else if(header[54] == 2 || header[54] == 3)
         cartridge_type = CARTRIDGE_TYPE_SUPERCART;
      else if(header[54] == 4 || header[54] == 5 || header[54] == 6 || header[54] == 7)
         cartridge_type = CARTRIDGE_TYPE_SUPERCART_RAM;
      else if(header[54] == 8 || header[54] == 9 || header[54] == 10 || header[54] == 11)
         cartridge_type = CARTRIDGE_TYPE_SUPERCART_ROM;
      else
         cartridge_type = CARTRIDGE_TYPE_NORMAL;
   }
   else
   {
      if(header[53] == 1)
         cartridge_type = CARTRIDGE_TYPE_ABSOLUTE;
      else if(header[53] == 2)
         cartridge_type = CARTRIDGE_TYPE_ACTIVISION;
      else if(header[53] == 16)
         cartridge_type = CARTRIDGE_TYPE_SOUPER;
      else
         cartridge_type = CARTRIDGE_TYPE_NORMAL;
   }

   cartridge_pokey = (header[54] & 1)? true: false;
   cartridge_controller[0] = header[55];
   cartridge_controller[1] = header[56];
   cartridge_region = header[57];
   cartridge_flags = 0;
   cartridge_bupchip = false;
}

// ----------------------------------------------------------------------------
// LoadROM
// ----------------------------------------------------------------------------
uint8_t cartridge_LoadROM(uint32_t address) {
   if(address >= cartridge_size)
      return 0;
   return cartridge_buffer[address];
}

// ----------------------------------------------------------------------------
// Load
// ----------------------------------------------------------------------------
bool cartridge_Load(bool persistent_data, const uint8_t* data, uint32_t size)
{
   int index;
   uint32_t offset     = 0;
   uint8_t header[128] = {0};

   /* Cartridge data is invalid. */
   if(size <= 128)
      return false;

   for(index = 0; index < 128; index++)
      header[index] = data[index];

   /* Prosystem doesn't support CC2 hacks. */
   if (cartridge_CC2(header))
      return false;

   if(cartridge_HasHeader(header))
   {
      cartridge_ReadHeader(header);
      size   -= 128;
      offset  = 128;
   }

   cartridge_size      = size;
   if (persistent_data)
      cartridge_buffer = (uint8_t*)data + offset;
   else
   {
      cartridge_buffer = (uint8_t*)malloc(cartridge_size * sizeof(uint8_t));

      for(index = 0; index < cartridge_size; index++)
         cartridge_buffer[index] = data[index + offset];
   }

   hash_Compute(cartridge_digest, cartridge_buffer, cartridge_size);

   return true;
}

// ----------------------------------------------------------------------------
// Store
// ----------------------------------------------------------------------------
void cartridge_Store(void)
{
   switch(cartridge_type)
   {
      case CARTRIDGE_TYPE_NORMAL:
         memory_WriteROM(65536 - cartridge_size, cartridge_size, cartridge_buffer);
         break;
      case CARTRIDGE_TYPE_SUPERCART:
         if(cartridge_GetBankOffset(7) < cartridge_size)
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(7));
         break;
      case CARTRIDGE_TYPE_SUPERCART_LARGE:
         if(cartridge_GetBankOffset(8) < cartridge_size)
         {
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(8));
            memory_WriteROM(16384, 16384, cartridge_buffer + cartridge_GetBankOffset(0));
         }
         break;
      case CARTRIDGE_TYPE_SUPERCART_RAM:
         if(cartridge_GetBankOffset(7) < cartridge_size)
         {
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(7));
            memory_ClearROM(16384, 16384);
         }
         break;
      case CARTRIDGE_TYPE_SUPERCART_ROM:
         if(cartridge_GetBankOffset(7) < cartridge_size && 
               cartridge_GetBankOffset(6) < cartridge_size)
         {
            memory_WriteROM(49152, 16384, cartridge_buffer + cartridge_GetBankOffset(7));
            memory_WriteROM(16384, 16384, cartridge_buffer + cartridge_GetBankOffset(6));
         }
         break;
      case CARTRIDGE_TYPE_ABSOLUTE:
         memory_WriteROM(16384, 16384, cartridge_buffer);
         memory_WriteROM(32768, 32768, cartridge_buffer + cartridge_GetBankOffset(2));
         break;
      case CARTRIDGE_TYPE_ACTIVISION:
         if(122880 < cartridge_size)
         {
            memory_WriteROM(40960, 16384, cartridge_buffer);
            memory_WriteROM(16384, 8192, cartridge_buffer + 106496);
            memory_WriteROM(24576, 8192, cartridge_buffer + 98304);
            memory_WriteROM(32768, 8192, cartridge_buffer + 122880);
            memory_WriteROM(57344, 8192, cartridge_buffer + 114688);
         }
         break;
      case CARTRIDGE_TYPE_SOUPER:
         memory_WriteROM(0xc000, 0x4000, cartridge_buffer + cartridge_GetBankOffset(31));
         memory_WriteROM(0x8000, 0x4000, cartridge_buffer + cartridge_GetBankOffset(0));
         memory_ClearROM(0x4000, 0x4000);
         break;
   }
}

// ----------------------------------------------------------------------------
// Write
// ----------------------------------------------------------------------------
void cartridge_Write(uint16_t address, uint8_t data)
{
   switch(cartridge_type)
   {
      case CARTRIDGE_TYPE_SUPERCART:
      case CARTRIDGE_TYPE_SUPERCART_RAM:
      case CARTRIDGE_TYPE_SUPERCART_ROM:
         if(address >= 32768 && address < 49152 && data < 9)
            cartridge_StoreBank(data);
         break;
      case CARTRIDGE_TYPE_SUPERCART_LARGE:
         if(address >= 32768 && address < 49152 && data < 9)
            cartridge_StoreBank(data + 1);
         break;
      case CARTRIDGE_TYPE_ABSOLUTE:
         if(address == 32768 && (data == 1 || data == 2))
            cartridge_StoreBank(data - 1);
         break;
      case CARTRIDGE_TYPE_ACTIVISION:
         if(address >= 65408)
            cartridge_StoreBank(address & 7);
         break;
    case CARTRIDGE_TYPE_SOUPER:
         if(address >= 0x4000 && address < 0x8000) {
            memory_souper_ram[memory_souper_GetRamAddress(address)] = data;
            break;
         }

         switch(address)
         {
            case CARTRIDGE_SOUPER_BANK_SEL:
               cartridge_StoreBank(data & 31);
               break;
            case CARTRIDGE_SOUPER_CHR_A_SEL:
               cartridge_souper_StoreChrBank(0, data);
               break;
            case CARTRIDGE_SOUPER_CHR_B_SEL:
               cartridge_souper_StoreChrBank(1, data);
               break;
            case CARTRIDGE_SOUPER_MODE_SEL:
               cartridge_souper_SetMode(data);
               break;
            case CARTRIDGE_SOUPER_EXRAM_V_SEL:
               cartridge_souper_SetRamPageBank(0, data);
               break;
            case CARTRIDGE_SOUPER_EXRAM_D_SEL:
               cartridge_souper_SetRamPageBank(1, data);
               break;
            case CARTRIDGE_SOUPER_AUDIO_CMD:
               bupchip_ProcessAudioCommand(data);
               break;
         }
         break;
   }

   if(cartridge_pokey && address >= 0x4000 && address < 0x4009)
   {
      switch(address)
      {
         case POKEY_AUDF1:
            pokey_SetRegister(POKEY_AUDF1, data);
            break;
         case POKEY_AUDC1:
            pokey_SetRegister(POKEY_AUDC1, data);
            break;
         case POKEY_AUDF2:
            pokey_SetRegister(POKEY_AUDF2, data);
            break;
         case POKEY_AUDC2:
            pokey_SetRegister(POKEY_AUDC2, data);
            break;
         case POKEY_AUDF3:
            pokey_SetRegister(POKEY_AUDF3, data);
            break;
         case POKEY_AUDC3:
            pokey_SetRegister(POKEY_AUDC3, data);
            break;
         case POKEY_AUDF4:
            pokey_SetRegister(POKEY_AUDF4, data);
            break;
         case POKEY_AUDC4:
            pokey_SetRegister(POKEY_AUDC4, data);
            break;
         case POKEY_AUDCTL:
            pokey_SetRegister(POKEY_AUDCTL, data);
            break;
      }
   }
}

// ----------------------------------------------------------------------------
// StoreBank
// ----------------------------------------------------------------------------
void cartridge_StoreBank(uint8_t bank)
{
   switch(cartridge_type)
   {
      case CARTRIDGE_TYPE_SUPERCART:
         cartridge_WriteBank(32768, bank);
         break;
      case CARTRIDGE_TYPE_SUPERCART_RAM:
         cartridge_WriteBank(32768, bank);
         break;
      case CARTRIDGE_TYPE_SUPERCART_ROM:
         cartridge_WriteBank(32768, bank);
         break;
      case CARTRIDGE_TYPE_SUPERCART_LARGE:
         cartridge_WriteBank(32768, bank);        
         break;
      case CARTRIDGE_TYPE_ABSOLUTE:
         cartridge_WriteBank(16384, bank);
         break;
      case CARTRIDGE_TYPE_ACTIVISION:
         cartridge_WriteBank(40960, bank);
         break;
      case CARTRIDGE_TYPE_SOUPER:
         cartridge_WriteBank(32768, bank);
         break;
   }  
}

// ----------------------------------------------------------------------------
// IsLoaded
// ----------------------------------------------------------------------------
bool cartridge_IsLoaded(void)
{
  return (cartridge_buffer != NULL)? true: false;
}

// ----------------------------------------------------------------------------
// Release
// ----------------------------------------------------------------------------
void cartridge_Release(bool persistent_data)
{
   if (!persistent_data)
   {
      if (cartridge_buffer)
         free(cartridge_buffer);
   }
   cartridge_buffer = NULL;
   cartridge_size   = 0;
}
