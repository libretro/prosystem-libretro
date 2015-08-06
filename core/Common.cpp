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
// Common.cpp
// ----------------------------------------------------------------------------
#include <algorithm>
#include <stdio.h>
#include "Common.h"

std::string common_defaultPath;

// ----------------------------------------------------------------------------
// ParseUint
// ----------------------------------------------------------------------------
uint32_t common_ParseUint(std::string text)
{
   return (uint32_t)atoi(text.c_str( ));
}

// ----------------------------------------------------------------------------
// ParseWord
// ----------------------------------------------------------------------------
uint16_t common_ParseWord(std::string text)
{
   return (uint16_t)atoi(text.c_str( ));
}

// ----------------------------------------------------------------------------
// ParseByte
// ----------------------------------------------------------------------------
uint8_t common_ParseByte(std::string text)
{
   return (uint8_t)atoi(text.c_str( ));
}

// ----------------------------------------------------------------------------
// ParseBool
// ----------------------------------------------------------------------------
bool common_ParseBool(std::string text)
{
   if(text.compare("true") == 0 || text.compare("TRUE") == 0 || text.compare("True") == 0)
      return true;
   if(atoi(text.c_str( )) == 1)
      return true;
   return false;
}

// ----------------------------------------------------------------------------
// Remove
// ----------------------------------------------------------------------------
std::string common_Remove(std::string target, char value)
{
   target.erase(std::remove(target.begin(), target.end(), value), target.end());
   return target;
}
