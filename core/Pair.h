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
 * Pair.h
 * ----------------------------------------------------------------------------
 */
#ifndef PAIR_H
#define PAIR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

union Pair 
{
   uint16_t w;
   struct Join
   {
#ifdef MSB_FIRST
      uint8_t h; 
      uint8_t l;
#else
      uint8_t l; 
      uint8_t h;
#endif
   } b;
};

typedef union Pair pair;

#ifdef __cplusplus
}
#endif

#endif
