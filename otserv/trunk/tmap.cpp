//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// the map of OpenTibia
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundumpion; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundumpion,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
// $Id$
//////////////////////////////////////////////////////////////////////
// $Log$
// Revision 1.14  2003/10/17 22:25:02  tliffrag
// Addes SorryNotPossible; added configfile; basic lua support
//
// Revision 1.13  2003/09/25 21:17:52  timmit
// Adding PlayerList in TMap and getID().  Not workigng!
//
// Revision 1.12  2003/09/23 20:00:51  tliffrag
// added !g command
//
// Revision 1.11  2003/09/23 16:41:19  tliffrag
// Fixed several map bugs
//
// Revision 1.10  2003/09/18 12:35:22  tliffrag
// added item dragNdrop
//
// Revision 1.9  2003/09/17 16:35:08  tliffrag
// added !d command and fixed lag on windows
//
// Revision 1.8  2003/09/08 13:28:41  tliffrag
// Item summoning and maploading/saving now working.
//
// Revision 1.7  2003/08/26 21:09:53  tliffrag
// fixed maphandling
//
// Revision 1.6  2003/08/25 21:28:12  tliffrag
// Fixed all warnings.
//
// Revision 1.5  2003/05/19 16:48:37  tliffrag
// Loggingin, talking, walking around, logging out working
//
// Revision 1.4  2002/05/28 13:55:57  shivoc
// some minor changes
//
// Revision 1.3  2002/04/08 15:57:03  shivoc
// made some changes to be more ansi compliant
//
// Revision 1.2  2002/04/08 14:12:49  shivoc
// fixed a segfault because of out of bounds access to tiles array
//
// Revision 1.1  2002/04/08 13:53:59  acrimon
// Added some very basic map support
//
//////////////////////////////////////////////////////////////////////

#include <iostream>
#include "items.h"
#include "tmap.h"
#include "player.h"
#include "npc.h"
#include <stdio.h>

int Tile::getStackPosItem(){
return 0;
}

bool Tile::isBlocking(){
	bool blocking=false;
	std::list<Item *>::iterator i;
	//if any of the items on this tile is blocking,
	//the whole tile blocks
	for(i = this->begin(); i != this->end(); i++){
		if((*i)->isBlocking())
			blocking=true;
	}
	if(creature) //creatures also block a tile
		blocking=true;
	return blocking;
}

int Tile::getStackPosPlayer(){
	bool top=false;
	std::list<Item *>::iterator i;
	//if any of the items on this tile is blocking,
	//the whole tile blocks
	for(i = this->begin(); i != this->end(); i++){
		if((*i)->isAlwaysOnTop())
			top=true;
	}
	if(top)
		return 2;
	else
		return 1;
}

std::string Tile::getDescription(){
	std::string ret;

	ret="You dont know why, but you cant see anything!";
	return ret;
}

Map::Map() {
	//this code is ugly but works
	//TODO improve this code to support things like
	//a quadtree to speed up everything
	#ifdef __DEBUG__
	std::cout << "Loading map" << std::endl;
	#endif
	FILE* dump=fopen("otserv.map", "rb");
	if(!dump){
		 #ifdef __DEBUG__
		 std::cout << "Fatal error: Mapfile not found" << std::endl;
		 #endif
		exit(1);
	}
	position topleft, bottomright, now;


	topleft.x=fgetc(dump)*256;	topleft.x+=fgetc(dump);
	topleft.y=fgetc(dump)*256;	topleft.y+=fgetc(dump);
	topleft.z=fgetc(dump);

	bottomright.x=fgetc(dump)*256;	bottomright.x+=fgetc(dump);
	bottomright.y=fgetc(dump)*256;	bottomright.y+=fgetc(dump);
	bottomright.z=fgetc(dump);

	int xsize= bottomright.x-topleft.x;
	int ysize= bottomright.y-topleft.y;
	//TODO really place this map patch where it belongs

	for(int y=0; y < ysize; y++){
		for(int x=0; x < xsize; x++){
		  tiles[x][y] = new Tile;
			while(true){
				int id=fgetc(dump)*256;id+=fgetc(dump);
				if(id==0x00FF)
					break;
				now.x=x+MINX;now.y=y+MINY;now.z=topleft.z;
				tiles[x][y]->push_back(new Item(id));
			}
		}
	}
	fclose(dump);
//	tiles[32864-MINX][32863-MINY]->creature=new NPC("ruediger");
}

Map::Map(char *filename) {
    // load the map from a file
}

Tile *Map::tile(unsigned short _x, unsigned short _y, unsigned char _z) {
    return tiles[_x - MINX][_y - MINY];
}

Map::~Map() {

}

int Map::saveMap(){
	//save the map
	#ifdef __DEBUG__
	std::cout << "Saving the map" << std::endl;
	#endif
	FILE* dump=fopen("otserv.map", "wb+");
	//first dump position of top left corner, then bottom right corner
	//then the raw map-dumpa
	fputc((int)(MINX/256), dump);
	fputc((int)(MINX%256), dump);
	fputc((int)(MINY/256), dump);
	fputc((int)(MINY%256), dump);
	fputc( (int) 7, dump);

	fputc((int)(MAXX/256), dump);
	fputc((int)(MAXX%256), dump);
	fputc( (int)(MAXY/256), dump);
	fputc((int)(MAXY%256), dump);
	fputc((int) 7, dump);

	for(int y=0; y < MAXY-MINY; y++){
		for(int x=0; x < MAXX-MINX; x++){
			Tile* tile=tiles[x][y];
			for (Item::iterator it=tile->begin(); it != tile->end(); it++) {
				fputc((int)( (*it)->getID()/256), dump);
				fputc((int)( (*it)->getID()%256), dump);
			}
			fputc(0x00, dump);
			fputc(0xFF, dump); // tile end
		}
	}
	fclose(dump);
	return true;
}

Creature* Map::getPlayerByID( unsigned long id ){
  std::list<Creature*>::iterator i;
  for( i = PlayerList.begin(); i != PlayerList.end(); i++ )
  {
    if( (*i)->getID() == id )
    {
      return *i;
    }
  }
  return NULL; //just in case the player doesnt exist
}

position Map::placeCreature(position pos, Creature* c){
  if( tiles[pos.x-MINX][pos.y-MINY]->isBlocking()){
  	//crap we need to find another spot
	pos.x++;
	return placeCreature(pos, c);
  }
  tiles[pos.x-MINX][pos.y-MINY]->creature=c;
  //we placed the creature, now tell everbody who saw it about the login
  Action* a = new Action;
  a->type=ACTION_LOGIN;
  a->pos1=pos;
  a->creature=c;
  distributeAction(pos, a);
  return pos;
}

int Map::removeCreature(position pos){
  if(tiles[pos.x-MINX][pos.y-MINY]->creature)
    tiles[pos.x-MINX][pos.y-MINY]->creature=NULL;
  //now distribute the action
  Action* a= new Action;
  a->type=ACTION_LOGOUT;
  a->pos1=pos;
  distributeAction(pos, a);
  delete a;
  return true;
}

int Map::requestAction(Creature* c, Action* a){
	//TODO us a switch here
	if(a->type == ACTION_TURN){
		//no checking needs to be done, we can always turn
		distributeAction(a->pos1, a);
	}
	if(a->type==ACTION_MOVE){
		a->pos2=a->pos1;
		if(a->direction%2){
			int x=a->direction-2;
			a->pos2.x-=x;
		}
		else{
			int y=a->direction-1;
			a->pos2.y+=y;
		}
		//we got the new action, now distribute it
		//FIXME THIS IS A BUG!!!
			#ifdef __DEBUG__
			std::cout << "Going to distribute an action" << std::endl;
			#endif
		//we move the pointer
		if(tiles[a->pos2.x-MINX][a->pos2.y-MINY]->isBlocking())
			return TMAP_ERROR_TILE_OCCUPIED;
		a->stack=tiles[a->pos1.x-MINX][a->pos1.y-MINY]->getStackPosPlayer();
		tiles[a->pos2.x-MINX][a->pos2.y-MINY]->creature=tiles[a->pos1.x-MINX][a->pos1.y-MINY]->creature;
		tiles[a->pos1.x-MINX][a->pos1.y-MINY]->creature=NULL;
		a->creature=c;
		distributeAction(a->pos1, a);
	}
	if(a->type==ACTION_SAY){
		//says should be ok most of the time
		distributeAction(a->pos1, a);
	}
	if(a->type==ACTION_THROW){
	//FIXME we should really check, if the player can throw
		Tile* tile=tiles[a->pos1.x-MINX][a->pos1.y-MINX];
		Item::iterator it=tile->end(); it--;
		if(a->pos2.x!=0xFFFF && a->pos1.x!=0xFFFF)
		//if start is on ground and end is on ground
		summonItem(a->pos2, (*it)->getID());
		if(a->pos2.x!=0xFFFF && a->pos1.x==0xFFFF)
		;//if end is on ground and start is in equipement
		if(a->pos1.x!=0xFFFF)
		//if start is on ground
		;//removeItem(a->pos1);
		else{
		//if start is in equipement
			//take the item away
		}
		std::cout << "i should move " << a->count << " items" << std::endl;
	}
	return true;
}

int Map::distributeAction(position pos, Action* a){
	//finds out how saw a certain action and tells him about it
	//the region in which we can be seen is
	// -9/-7 to 9/7
	std::list<Creature*> victims;
	for(int x=-7; x <=7; x++){
		for(int y=-6; y <=6; y++){
			if(tiles[a->pos1.x+x-MINX][a->pos1.y+y-MINY]->creature){
				victims.push_back(tiles[a->pos1.x+x-MINX][a->pos1.y+y-MINY]->creature);
			}
		}
	}
	for(int x=-7; x <=7; x++){
		for(int y=-6; y <=6; y++){
			if(tiles[a->pos2.x+x-MINX][a->pos2.y+y-MINY]->creature){
				victims.push_back(tiles[a->pos2.x+x-MINX][a->pos2.y+y-MINY]->creature);
			}
		}
	}
	victims.sort();
	victims.unique();
	std::list<Creature*>::iterator i;
	for(i=victims.begin(); i!=victims.end();++i)
		(*i)->sendAction(a);
	delete a;
	return true;
}

int Map::summonItem(Action* a){
	Item* item=new Item(a->id);
	if(item->isStackable() && !a->count)
		return TMAP_ERROR_NO_COUNT;
	if(!a->id)
		return false;
	#ifdef __DEBUG__
	std::cout << "Summoning item with id " << a->id << std::endl;
	#endif
	item->count=a->count;
	tiles[a->pos1.x-MINX][a->pos1.y-MINY]->push_back(item);
	Action* b= new Action;
	b->type=ACTION_ITEM_APPEAR;
	b->pos1=a->pos1;
	b->id=a->id;
	b->count=a->count;
	distributeAction(a->pos1, b);
	return true;
}

int Map::summonItem(position pos, int id){
	if(!id)
		return false;
	#ifdef __DEBUG__
	std::cout << "Summoning item with id " << id << std::endl;
	#endif
	tiles[pos.x-MINX][pos.y-MINY]->push_back(new Item(id));
	Action* a= new Action;
	a->type=ACTION_ITEM_APPEAR;
	a->pos1=pos;
	a->id=id;
	distributeAction(pos, a);
	return true;
}

int Map::changeGround(position pos, int id){
  if(!id)
    return false;
#ifdef __DEBUG__
  std::cout << "Summoning item with id " << id << std::endl;
#endif
  tiles[pos.x-MINX][pos.y-MINY]->pop_front();
  tiles[pos.x-MINX][pos.y-MINY]->push_front(new Item(id));
  Action* a= new Action;
  a->type=ACTION_GROUND_CHANGE;
  a->pos1=pos;
  a->id=id;
  distributeAction(pos, a);
  return true;
}

int Map::removeItem(position pos){
	Tile* tile=tiles[pos.x-MINX][pos.y-MINY];
	if(tile->size()<=1)
		return false;
	tile->pop_back();
	Action* a= new Action;
	a->type=ACTION_ITEM_DISAPPEAR;
	if(tile->creature)
	  a->stack=2;
	else
	  a->stack=1;
	a->pos1=pos;
	distributeAction(pos, a);
	return true;
}
