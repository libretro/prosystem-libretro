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
// Logger.cpp
// ----------------------------------------------------------------------------
#include "Logger.h"

// ----------------------------------------------------------------------------
// Log
// ----------------------------------------------------------------------------
static void logger_Log(std::string message, uint8_t level, std::string source)
{
   switch(level)
   {
      case LOGGER_LEVEL_ERROR:
         break;
      case LOGGER_LEVEL_INFO:
         break;
      default:
         break;
   }
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
bool logger_Initialize( ) {
   return true;
}

// ----------------------------------------------------------------------------
// LogError //////////
// ----------------------------------------------------------------------------
void logger_LogError(int message, std::string source) {
}

// ----------------------------------------------------------------------------
// LogError    
// ----------------------------------------------------------------------------
void logger_LogError(std::string message, std::string source) {
}


// ----------------------------------------------------------------------------
// LogInfo
// ----------------------------------------------------------------------------
void logger_LogInfo(int message, std::string source) {
}

// ----------------------------------------------------------------------------
// LogInfo /////////
// ----------------------------------------------------------------------------
void logger_LogInfo(std::string message, std::string source) {
}

// ----------------------------------------------------------------------------
// LogDebug ////////////
// ----------------------------------------------------------------------------
void logger_LogDebug(int message, std::string source) {
}

// ----------------------------------------------------------------------------
// LogDebug
// ----------------------------------------------------------------------------
void logger_LogDebug(std::string message, std::string source) {
}

// ----------------------------------------------------------------------------
// Release
// ----------------------------------------------------------------------------
void logger_Release( ) {
}
