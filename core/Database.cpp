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
// Database.cpp
// ----------------------------------------------------------------------------
#include "Database.h"
#include "Cartridge.h"
#include "Common.h"
#define DATABASE_SOURCE "Database.cpp"

bool database_enabled = true;
std::string database_filename;

static std::string database_GetValue(std::string entry)
{
   int index = entry.rfind('=');
   return entry.substr(index + 1);
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
void database_Initialize(void)
{
   database_filename = common_defaultPath + "ProSystem.dat";
}

typedef struct cartridge_db
{
   int8_t digest[256];
   char title[256];
   uint8_t type;
   bool pokey;
   uint8_t controller1;
   uint8_t controller2;
   uint8_t region;
   uint32_t flags;
   int8_t crossx;
   int8_t crossy;
   uint8_t hblank;
} cartridge_db_t;

static const cartridge_db db_list[] = 
{
   {
      "4332c24e4f3bc72e7fe1b77adf66c2b7",  /* digest */
      "3D Asteroids",                      /* title */
      0,                                   /* type */
      false,                               /* pokey */
      1,                                   /* controller 1 */
      1,                                   /* controller 2 */
      0,                                   /* region */
      0,                                   /* flags */
      0,                                   /* crossx */
      0,                                   /* crossy */
      0                                    /* hblank */
   },
   {
      "0be996d25144966d5541c9eb4919b289",  /* digest */
      "Ace Of Aces",                       /* title */
      4,                                   /* type */
      false,                               /* pokey */
      1,                                   /* controller 1 */
      1,                                   /* controller 2 */
      0,                                   /* region */
      0,                                   /* flags */
      0,                                   /* crossx */
      0,                                   /* crossy */
      0,                                   /* hblank */
   },
   {
      "aadde920b3aaba03bc10b40bd0619c94",  /* digest */
      "Ace Of Aces",                       /* title */
      4,                                   /* type */
      false,                               /* pokey */
      1,                                   /* controller 1 */
      1,                                   /* controller 2 */
      1,                                   /* region */
      0,                                   /* flags */
      0,                                   /* crossx */
      0,                                   /* crossy */
      0                                    /* hblank */
   },
   {
      "877dcc97a775ed55081864b2dbf5f1e2",  /* digest */
      "Alien Brigade",                     /* title */
      2,                                   /* type */
      false,                               /* pokey */
      3,                                   /* controller 1 */
      3,                                   /* controller 2 */
      0,                                   /* region */
      0,                                   /* flags */
      15,                                  /* crossx */
      15,                                  /* crossy */
      0                                    /* hblank */
   },
   {
      "de3e9496cb7341f865f27e5a72c7f2f5",  /* digest */
      "Alien Brigade",                     /* title */
      2,                                   /* type */
      false,                               /* pokey */
      3,                                   /* controller 1 */
      3,                                   /* controller 2 */
      1,                                   /* region */
      0,                                   /* flags */
      15,                                  /* crossx */
      -20,                                 /* crossy */
      0                                    /* hblank */
   },
   {
      "404f95103b70975a42cb09946dc3adca",
      "Apple Snaffle (Jul 17-Rev 24) (2009)",
      3,
      true,
      1,
      1,
      0,
      0,
      0,
      0,
      0                                    /* hblank */
   },
   {
      "07342c78619ba6ffcc61c10e907e3b50",
      "Asteroids",
      0,
      false,
      1,
      1,
      0,
      0,
      0,
      0,
      0 
   },
   {
      "8fc3a695eaea3984912d98ed4a543376",
      "Ballblazer",
      0,
      true,
      1,
      1,
      0,
      0,
      0,
      0,
      28
   },
   {
      "b558814d54904ce0582e2f6a801d03af",
      "Ballblazer",
      0,
      true,
      1,
      1,
      1,
      0,
      0,
      0,
      28
   },
   {
      "42682415906c21c6af80e4198403ffda",
      "Barnyard Blaster",
      1,
      true,
      2,
      1,
      0,
      0,
      0,
      10,
      0
   },
   {
      "babe2bc2976688bafb8b23c192658126",
      "Barnyard Blaster",
      1,
      true,
      2,
      1,
      1,
      0,
      0,
      12,
      0
   },
   {
      "f5f6b69c5eb4b55fc163158d1a6b423e",
      "Basketbrawl",
      4,
      false,
      1,
      1,
      0,
      0,
      0,
      0,
      0
   },
   {
      "fba002089fcfa176454ab507e0eb76cb",
      "Basketbrawl",
      4, 
      false,
      1,
      1,
      1,
      0,
      0,
      0,
      0
   },
   {
      "3e63be18e480fa63fce5e4c823286e53",
      "Beef Drop",
      0,
      true,
      1,
      1,
      1,
      0,
      0,
      0,
      0
   },
   {
      "5a09946e57dbe30408a8f253a28d07db",
      "Centipede",
      0,
      false,
      1,
      1,
      0,
      0,
      0,
      0,
      0
   },
   {
      "38c056a48472d9a9e16ebda5ed91dae7",
      "Centipede",
      0,
      false,
      1,
      1,
      1,
      0,
      0,
      0,
      0
   },
   {
      "93e4387864b014c155d7c17877990d1e",
      "Choplifter",
      0,
      false,
      1,
      1,
      0,
      0,
      0,
      0,
      0
   },
   {
      "59d4edb0230b5acc918b94f6bc94779f",
      "Choplifter",
      0,
      false,
      1,
      1,
      1,
      0,
      0,
      0,
      0
   },
   {
      "2e8e28f6ad8b9b9267d518d880c73ebb",
      "Commando",
      1,
      true,
      1,
      1,
      0,
      0,
      0,
      0,
      0
   },
   {
      "55da6c6c3974d013f517e725aa60f48e",
      "Commando",
      1,
      true,
      1,
      1,
      1,
      0,
      0,
      0,
      0
   },
   {
      "db691469128d9a4217ec7e315930b646",
      "Crack'ed",
      1,
      false,
      1,
      1,
      0,
      0,
      0,
      0,
      0
   },
   {
      "7cbe78fa06f47ba6516a67a4b003c9ee",
      "Crack'ed",
      1,
      false,
      1,
      1,
      1,
      0,
      0,
      0,
      0
   },
   {
      "a94e4560b6ad053a1c24e096f1262ebf",
      "Crossbow",
      2,
      false,
      3,
      3,
      0,
      0,
      15,
      10,
      0
   },
      {
         "63db371d67a98daec547b2abd5e7aa95",
         "Crossbow",
         2,
         false,
         3,
         3,
         1,
         0,
         15,
         5,
         0
      },
      {
         "179b76ff729d4849b8f66a502398acae",
         "Dark Chambers",
         1,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "a2b8e2f159642c4b91de82e9a2928494",
         "Dark Chambers",
         1,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "95ac811c7d27af0032ba090f28c107bd",
         "Desert Falcon",
         0,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "2d5d99b993a885b063f9f22ce5e6523d",
         "Desert Falcon",
         0,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "731879ea82fc0ca245e39e036fe293e6",
         "Dig Dug",
         0,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "408dca9fc40e2b5d805f403fa0509436",
         "Dig Dug",
         0,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "5e332fbfc1e0fc74223d2e73271ce650",
         "Donkey Kong Jr",
         0,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "4dc5f88243250461bd61053b13777060",
         "Donkey Kong Jr",
         0,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "19f1ee292a23636bd57d408b62de79c7",
         "Donkey Kong",
         0,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "8e96ef14ce9b5d84bcbc996b66d6d4c7",
         "Donkey Kong",
         0,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "543484c00ba233736bcaba2da20eeea9",
         "Double Dragon",
         6,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "de2ebafcf0e37aaa9d0e9525a7f4dd62",
         "Double Dragon",
         6,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "2251a6a0f3aec84cc0aff66fc9fa91e8",
         "F-18 Hornet",
         5,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "e7709da8e49d3767301947a0a0b9d2e6",
         "F-18 Hornet",
         5,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "d25d5d19188e9f149977c49eb0367cd1",
         "Fatal Run",
         4,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "23505651ac2e47f3637152066c3aa62f",
         "Fatal Run",
         4,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "07dbbfe612a0a28e283c01545e59f25e",
         "Fight Night",
         4,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "e80f24e953563e6b61556737d67d3836",
         "Fight Night",
         4,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "cf76b00244105b8e03cdc37677ec1073",
         "Food Fight",
         0,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "de0d4f5a9bf1c1bddee3ed2f7ec51209",
         "Food Fight",
         0,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "fb8d803b328b2e442548f7799cfa9a4a",
         "Galaga",
         0,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      },
      {
         "f5dc7dc8e38072d3d65bd90a660148ce",
         "Galaga",
         0,
         false,
         1,
         1,
         1,
         0,
         0,
         0,
         0
      },
      {
         "06204dadc975be5e5e37e7cc66f984cf",
         "Gato",
         0,
         false,
         1,
         1,
         0,
         0,
         0,
         0,
         0
      }
};

// ----------------------------------------------------------------------------
// Load
// ----------------------------------------------------------------------------
bool database_Load(const char *digest)
{
   if(database_enabled)
   {
      FILE* file = fopen(database_filename.c_str( ), "r");
      if(file == NULL)
         return false;  

      char buffer[256];
      while(fgets(buffer, 256, file) != NULL)
      {
         std::string line = buffer;
         if(line.compare(1, 32, digest) == 0)
         {
            int index;
            std::string entry[7];

            for(index = 0; index < 7; index++)
            {
               fgets(buffer, 256, file);
               entry[index] = common_Remove(buffer, '\n');  
               entry[index] = common_Remove(entry[index], '\r');
            }

            cartridge_type = common_ParseByte(database_GetValue(entry[1]).c_str());
            cartridge_pokey = common_ParseBool(database_GetValue(entry[2]));
            cartridge_controller[0] = common_ParseByte(database_GetValue(entry[3]).c_str());
            cartridge_controller[1] = common_ParseByte(database_GetValue(entry[4]).c_str());
            cartridge_region = common_ParseByte(database_GetValue(entry[5]).c_str());
            cartridge_flags = common_ParseUint(database_GetValue(entry[6]).c_str());
            break;
         }
      }    

      fclose(file);  
   }

   return true;
}
