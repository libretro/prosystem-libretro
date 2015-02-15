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
// Format
// ----------------------------------------------------------------------------
std::string common_Format(double value)
{
  return common_Format(value, "%f");
}

// ----------------------------------------------------------------------------
// Format
// ----------------------------------------------------------------------------
std::string common_Format(double value, std::string specification)
{
  char buffer[17] = {0};
  sprintf(buffer, specification.c_str( ), value);
  return std::string(buffer);
}

// ----------------------------------------------------------------------------
// Format
// ----------------------------------------------------------------------------
std::string common_Format(uint value)
{
   char buffer[11] = {0};
   sprintf(buffer, "%d", value);
   return std::string(buffer);
}

// ----------------------------------------------------------------------------
// Format
// ----------------------------------------------------------------------------
std::string common_Format(word value)
{
   char buffer[6] = {0};
   sprintf(buffer, "%d", value);
   return std::string(buffer);
}

// ----------------------------------------------------------------------------
// Format
// ----------------------------------------------------------------------------
std::string common_Format(byte value)
{
   char buffer[4] = {0};
   sprintf(buffer, "%d", value);
   return std::string(buffer);
}

// ----------------------------------------------------------------------------
// Format
// ----------------------------------------------------------------------------
std::string common_Format(bool value)
{
   return (value)? "true": "false";
}

// ----------------------------------------------------------------------------
// ParseUint
// ----------------------------------------------------------------------------
uint common_ParseUint(std::string text)
{
   return (uint)atoi(text.c_str( ));
}

// ----------------------------------------------------------------------------
// ParseWord
// ----------------------------------------------------------------------------
word common_ParseWord(std::string text)
{
   return (word)atoi(text.c_str( ));
}

// ----------------------------------------------------------------------------
// ParseByte
// ----------------------------------------------------------------------------
byte common_ParseByte(std::string text)
{
   return (byte)atoi(text.c_str( ));
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
// Trim
// ----------------------------------------------------------------------------
std::string common_Trim(std::string target)
{
   int index;
   for(index = target.length( ) - 1; index >= 0; index--)
   {
      if(target[index] != 0x20 && target[index] != '\n')
         break;
   }
   return target.substr(0, index + 1);
}

// ----------------------------------------------------------------------------
// Remove
// ----------------------------------------------------------------------------
std::string common_Remove(std::string target, char value)
{
   target.erase(std::remove(target.begin(), target.end(), value), target.end());
   return target;
}

// ----------------------------------------------------------------------------
// Replace (string, from, to)
// ----------------------------------------------------------------------------
std::string common_Replace(std::string target, char value1, char value2)
{
   int length = 0;
   int index;
   for(index = 0; index < target.size( ); index++)
      length++;

   char* buffer = new char[length + 1];
   for(index = 0; index < target.size( ); index++)
   {
      if(target[index] != value1)
         buffer[index] = target[index];
      else
         buffer[index] = value2;
   }

   buffer[length] = 0;
   std::string source = buffer;
   delete[ ] buffer;
   return source;
}

// ----------------------------------------------------------------------------
// GetExtension
// ----------------------------------------------------------------------------
std::string common_GetExtension(std::string filename)
{
   int position = filename.rfind('.');
   if(position != -1)
      return filename.substr(position);
   return "";
}
