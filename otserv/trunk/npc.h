//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
// $Id$
//////////////////////////////////////////////////////////////////////
// $Log$
// Revision 1.1  2003/10/17 22:25:02  tliffrag
// Addes SorryNotPossible; added configfile; basic lua support
//
//////////////////////////////////////////////////////////////////////

#ifndef __npc_h_
#define __npc_h_

#include "definitions.h"
#include "creature.h"
#include "protokoll.h"
extern "C"{
#include <lua.h>
#include <lauxlib.h>
}
#include <string>

//////////////////////////////////////////////////////////////////////
// Defines an NPC...
class NPC : public Creature {
  
 public:
  
  // our constructor
  NPC(const std::string script);
  
  // virtual destructor to be overloaded...
  virtual ~NPC();
  
  bool isPlayer();
  
  void sendAction(Action*);
  
  std::string getName(){
    return name;
  }
  void setMap(position,Map&) throw(texception);
  
  // we need our name and password...
  std::string name;
  lua_State* lua;

};


#endif // __npc_h_
