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
// Common.h
// ----------------------------------------------------------------------------
#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <stdint.h>
#include <stdlib.h>

extern std::string common_Remove(std::string target, char value);
extern uint32_t common_ParseUint(const char *text);
extern uint16_t common_ParseWord(const char *text);
extern uint8_t common_ParseByte(const char *text);
extern bool common_ParseBool(std::string text);
extern std::string common_defaultPath;

#endif
