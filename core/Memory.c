/* ----------------------------------------------------------------------------
 *   ___  ___  ___  ___       ___  ____  ___  _  _
 *  /__/ /__/ /  / /__  /__/ /__    /   /_   / |/ /
 * /    / \  /__/ ___/ ___/ ___/   /   /__  /    /  emulator
 *
 * ----------------------------------------------------------------------------
 * Copyright 2005 Greg Stanton
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 * Memory.c
 * ----------------------------------------------------------------------------
 */
#include <stdlib.h>
#include "Memory.h"
#include "Equates.h"
#include "Bios.h"
#include "Cartridge.h"
#include "Tia.h"
#include "Riot.h"

uint8_t memory_ram[MEMORY_SIZE] = {0};
uint8_t memory_rom[MEMORY_SIZE] = {0};
uint8_t memory_souper_ram[MEMORY_SOUPER_EXRAM_SIZE] = {0};

void memory_Reset(void)
{
   uint32_t index;
   for(index = 0; index < MEMORY_SIZE; index++)
   {
      memory_ram[index] = 0;
      memory_rom[index] = 1;
   }
   for(index = 0; index < 16384; index++)
      memory_rom[index] = 0;
}

uint16_t memory_souper_GetRamAddress(uint16_t address)
{
  uint8_t page = (address - 0x4000) >> 12;
  if((cartridge_souper_mode & CARTRIDGE_SOUPER_MODE_EXS) != 0)
  {
    if(address >= 0x6000 && address < 0x7000)
      page = cartridge_souper_ram_page_bank[0];
    else if(address >= 0x7000 && address < 0x8000)
      page = cartridge_souper_ram_page_bank[1];
  }
  return (address & 0x0fff) | ((uint16_t)page << 12);
}

uint8_t memory_Read(uint16_t address)
{
   switch ( address )
   {
      case INTIM:
      case INTIM | 0x2:
         memory_ram[INTFLG] &= 0x7f;
         return memory_ram[INTIM];
      case INTFLG:
      case INTFLG | 0x2:
         memory_ram[INTFLG] &= 0x7f;
         return memory_ram[INTFLG];
      default:
         if(cartridge_type == CARTRIDGE_TYPE_SOUPER && address >= 0x4000 && address < 0x8000)
            return memory_souper_ram[memory_souper_GetRamAddress(address)];
         break;
   }

   return memory_ram[address];
}

void memory_Write(uint16_t address, uint8_t data)
{
   if(!memory_rom[address])
   {
      switch(address)
      {
         case WSYNC:
            if(!(cartridge_flags & 128))
               memory_ram[WSYNC] = true;
            break;
         case INPTCTRL:
            if(data == 22 && cartridge_IsLoaded( )) 
               cartridge_Store( ); 
            else if(data == 2 && bios_enabled)
               bios_Store( );
            break;
         case INPT0:
         case INPT1:
         case INPT2:
         case INPT3:
         case INPT4:
         case INPT5:
            break;
         case AUDC0:
            tia_SetRegister(AUDC0, data);
            break;
         case AUDC1:
            tia_SetRegister(AUDC1, data);
            break;
         case AUDF0:
            tia_SetRegister(AUDF0, data);
            break;
         case AUDF1:
            tia_SetRegister(AUDF1, data);
            break;
         case AUDV0:
            tia_SetRegister(AUDV0, data);
            break;
         case AUDV1:
            tia_SetRegister(AUDV1, data);
            break;
         case SWCHA:	/*gdement:  Writing here actually writes to DRA inside the RIOT chip.
                       This value only indirectly affects output of SWCHA.  Ditto for SWCHB.*/
            riot_SetDRA(data);
            break;
         case SWCHB:
            riot_SetDRB(data);
            break;
         case TIM1T:
         case TIM1T | 0x8:
            riot_SetTimer(TIM1T, data);
            break;
         case TIM8T:
         case TIM8T | 0x8:
            riot_SetTimer(TIM8T, data);
            break;
         case TIM64T:
         case TIM64T | 0x8:
            riot_SetTimer(TIM64T, data);
            break;
         case T1024T:
         case T1024T | 0x8:
            riot_SetTimer(T1024T, data);
            break;
         default:
            if(cartridge_type == CARTRIDGE_TYPE_SOUPER && address >= 0x4000 && address < 0x8000)
            {
               memory_souper_ram[memory_souper_GetRamAddress(address)] = data;
               break;
            }
            memory_ram[address] = data;
            if(address >= 8256 && address <= 8447)
               memory_ram[address - 8192] = data;
            else if(address >= 8512 && address <= 8702)
               memory_ram[address - 8192] = data;
            else if(address >= 64 && address <= 255)
               memory_ram[address + 8192] = data;
            else if(address >= 320 && address <= 511)
               memory_ram[address + 8192] = data;
            break;
            /*TODO: gdement:  test here for debug port.  Don't put it in the switch because that will change behavior.*/
      }
   }
   else
      cartridge_Write(address, data);
}

void memory_WriteROM(uint16_t address, uint16_t size, const uint8_t* data)
{
   uint32_t index;

   if((address + size) <= MEMORY_SIZE && data != NULL)
   {
      for(index = 0; index < size; index++)
      {
         memory_ram[address + index] = data[index];
         memory_rom[address + index] = 1;
      }
   }
}

void memory_ClearROM(uint16_t address, uint16_t size)
{
   uint32_t index;

   if((address + size) <= MEMORY_SIZE)
   {
      for(index = 0; index < size; index++)
      {
         memory_ram[address + index] = 0;
         memory_rom[address + index] = 0;
      }
   }
}
