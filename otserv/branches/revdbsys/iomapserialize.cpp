//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// Base class for the map serialization
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
#include "otpch.h"

#include "definitions.h"
#include "iomapserialize.h"

#include "house.h"
#include "configmanager.h"
#include "luascript.h"
#include "game.h"

extern ConfigManager g_config;
extern Game g_game;

IOMapSerialize* IOMapSerialize::_instance = NULL;

IOMapSerialize* IOMapSerialize::getInstance()
{
	if(!_instance)
		_instance = new IOMapSerialize;

	return _instance;
}

IOMapSerialize::IOMapSerialize()
{
}

IOMapSerialize::~IOMapSerialize()
{
	//
}

bool IOMapSerialize::loadMap(Map* map, const std::string& identifier)
{
	Database* db = Database::instance();

	for(HouseMap::iterator it = Houses::getInstance().getHouseBegin(); it != Houses::getInstance().getHouseEnd(); ++it) {
		//load tile
		House* house = it->second;
		for(HouseTileList::iterator it = house->getHouseTileBegin(); it != house->getHouseTileEnd(); ++it)
			loadTile(*db, *it);
	}

	return true;
}

bool IOMapSerialize::saveMap(Map* map, const std::string& identifier)
{
	Database* db = Database::instance();
	DBQuery query;

	//Start the transaction
	if( !db->beginTransaction() )
		return false;

	//clear old tile data
	if(!db->executeQuery("DELETE FROM `tiles`")){
		db->rollback();
		return false;
	}

	uint32_t tileId = 0;

	for(HouseMap::iterator it = Houses::getInstance().getHouseBegin(); it != Houses::getInstance().getHouseEnd(); ++it){

		//save house items
		House* house = it->second;
		for(HouseTileList::iterator it = house->getHouseTileBegin(); it != house->getHouseTileEnd(); ++it){
			++tileId;
			if( !saveTile(db, tileId, *it) ) {
				db->rollback();
				return false;
			}
		}
	}

	//End the transaction
	return db->commit();
}

bool IOMapSerialize::saveTile(Database* db, uint32_t tileId, const Tile* tile)
{
	typedef std::list<std::pair<Container*, int> > ContainerStackList;
	typedef ContainerStackList::value_type ContainerStackList_Pair;
	ContainerStackList containerStackList;

	bool storedTile = false;
	int runningID = 0;
	Item* item = NULL;
	Container* container = NULL;

	int parentid = 0;
	DBQuery query;

	DBStatement* stmt = db->prepareStatement("INSERT INTO `tile_items` (`tile_id`, `sid`, `pid`, `itemtype`, `count`, `attributes`) VALUES (?, ?, ?, ?, ?, ?)");

	if(!stmt)
		return false;

	for(uint32_t i = 0; i < tile->getThingCount(); ++i){
		item = tile->__getThing(i)->getItem();

		if(!item)
			continue;

		if(!(!item->isNotMoveable() ||
			item->getDoor() ||
			(item->getContainer() && item->getContainer()->size() != 0)||
			(item->canWriteText())
			/*item->getBed()*/))
			continue;

		if(!storedTile){
			const Position& tilePos = tile->getPosition();
			query << "INSERT INTO `tiles` (`id`, `x`, `y`, `z`) VALUES (" << tileId
			<< ", " << tilePos.x
			<< ", " << tilePos.y
			<< ", " << tilePos.z
			<< ")";

			if( !db->executeQuery(query.str()) ) {
				db->freeStatement(stmt);
				return false;
			}

			storedTile = true;
		}
		++runningID;

		uint32_t attributesSize;

		PropWriteStream propWriteStream;
		item->serializeAttr(propWriteStream);
		const char* attributes = propWriteStream.getStream(attributesSize);

		stmt->setInt(1, tileId);
		stmt->setInt(2, runningID);
		stmt->setInt(3, parentid);
		stmt->setInt(4, (int32_t)item->getID() );
		stmt->setInt(5, (int)item->getItemCountOrSubtype() );
		stmt->bindStream(6, attributes, attributesSize);

		if(!stmt->execute()) {
			db->freeStatement(stmt);
			return false;
		}

		if(item->getContainer())
			containerStackList.push_back(ContainerStackList_Pair(item->getContainer(), runningID));
	}

	while(containerStackList.size() > 0) {
		ContainerStackList_Pair csPair = containerStackList.front();
		container = csPair.first;
		parentid = csPair.second;
		containerStackList.pop_front();

		for(ItemList::const_iterator it = container->getItems(); it != container->getEnd(); ++it) {
			item = (*it);
			++runningID;
			if(item->getContainer())
				containerStackList.push_back(ContainerStackList_Pair(item->getContainer(), runningID));

			uint32_t attributesSize;

			PropWriteStream propWriteStream;
			item->serializeAttr(propWriteStream);
			const char* attributes = propWriteStream.getStream(attributesSize);

			stmt->setInt(1, tileId);
			stmt->setInt(2, runningID);
			stmt->setInt(3, parentid);
			stmt->setInt(4, (int32_t)item->getID() );
			stmt->setInt(5, (int)item->getItemCountOrSubtype() );
			stmt->bindStream(6, attributes, attributesSize);

			if(!stmt->execute()) {
				db->freeStatement(stmt);
				return false;
			}
		}
	}

	return true;
}

bool IOMapSerialize::loadTile(Database& db, Tile* tile)
{
	typedef std::map<int,std::pair<Item*,int> > ItemMap;
	ItemMap itemMap;

	const Position& tilePos = tile->getPosition();

	DBQuery query;

	query << "SELECT `id` FROM `tiles` WHERE `x` = " << tilePos.x
	<< " AND `y` = " << tilePos.y
	<< " AND `z` = " << tilePos.z;

	DBResult* result = db.storeQuery(query.str());
	if(!result)
		return false;

	if(!result->next()) {
		db.freeResult(result);
		return false;
	}

	int tileId = result->getDataInt("id");
	db.freeResult(result);

	query.str("SELECT * FROM `tile_items` WHERE `tile_id` = ");
	query << tileId
	<< " ORDER BY `sid` DESC";
	if(result = db.storeQuery(query.str())) {
		Item* item = NULL;

		while(result->next()) {
			int sid = result->getDataInt("sid");
			int pid = result->getDataInt("pid");
			int type = result->getDataInt("itemtype");
			int count = result->getDataInt("count");
			item = NULL;

			unsigned long attrSize = 0;
			const char* attr = result->getDataStream("attributes", attrSize);
			PropStream propStream;
			propStream.init(attr, attrSize);

			const ItemType& iType = Item::items[type];
			if(iType.moveable || /* or object in a container*/ pid != 0){
				//create a new item
				item = Item::CreateItem(type, count);

				if(item){
					if(!item->unserializeAttr(propStream)){
						std::cout << "WARNING: Serialize error in IOMapSerialize::loadTile()" << std::endl;
					}

					if(pid == 0){
						tile->__internalAddThing(item);
						item->__startDecaying();
					}
				}
				else
					continue;
			}
			else{
				bool isDoor = iType.isDoor();

				//find this type in the tile
				for(uint32_t i = 0; i < tile->getThingCount(); ++i){
					Item* findItem = tile->__getThing(i)->getItem();

					if(!findItem)
						continue;

					if(findItem->getID() == type){
						item = findItem;
						if(!item->unserializeAttr(propStream)){
							std::cout << "WARNING: Serialize error in IOMapSerialize::loadTile()" << std::endl;
						}

						break;
					}
					else if(isDoor && findItem->getDoor()){
						item = findItem;
						item->setID(type);
					}
				}
			}

			if(item){
				std::pair<Item*, int> myPair(item, pid);
				itemMap[sid] = myPair;
			}
			else{
				std::cout << "WARNING: IOMapSerialize::loadTile(). NULL item at " << tile->getPosition() << ". type = " << type << ", sid = " << sid << ", pid = " << pid << std::endl;
			}
		}

		db.freeResult(result);
	}

	ItemMap::reverse_iterator it;
	ItemMap::iterator it2;

	for(it = itemMap.rbegin(); it != itemMap.rend(); ++it){
		Item* item = it->second.first;
		int pid = it->second.second;

		it2 = itemMap.find(pid);
		if(it2 != itemMap.end()){
			if(Container* container = it2->second.first->getContainer()){
				container->__internalAddThing(item);
				g_game.startDecay(item);
			}
		}
	}

	return true;
}

bool IOMapSerialize::loadHouseInfo(Map* map, const std::string& identifier)
{
	Database* db = Database::instance();
	DBQuery query;
	DBResult* result;

	if(result = db->storeQuery("SELECT * FROM `houses`"))
		return false;

	while(result->next()) {
		int houseid = result->getDataInt("id");
		House* house = Houses::getInstance().getHouse(houseid);
		if(house) {
			int ownerid = result->getDataInt("owner");
			int paid = result->getDataInt("paid");
			int payRentWarnings = result->getDataInt("warnings");

			house->setHouseOwner(ownerid);
			house->setPaidUntil(paid);
			house->setPayRentWarnings(payRentWarnings);
		}
	}

	db->freeResult(result);

	for(HouseMap::iterator it = Houses::getInstance().getHouseBegin(); it != Houses::getInstance().getHouseEnd(); ++it){
		House* house = it->second;
		if(house->getHouseOwner() != 0 && house->getHouseId() != 0) {
			query << "SELECT `listid`, `list` FROM `house_lists` WHERE `house_id` = " << house->getHouseId();
			if(result = db->storeQuery(query.str())) {
				while(result->next()) {
					int listid = result->getDataInt("listid");
					std::string list = result->getDataString("list");
					house->setAccessList(listid, list);
				}

				db->freeResult(result);
			}
		}
	}

	return true;
}

bool IOMapSerialize::saveHouseInfo(Map* map, const std::string& identifier)
{
	Database* db = Database::instance();
	DBQuery query;

	if( !db->beginTransaction() )
		return false;

	if(!db->executeQuery("DELETE FROM `houses`")) {
		db->rollback();
		return false;
	}

	DBStatement* stmt = db->prepareStatement("INSERT INTO `houses` (`id`, `owner`, `paid`, `warnings`) VALUES (?, ?, ?, ?)");

	for(HouseMap::iterator it = Houses::getInstance().getHouseBegin(); it != Houses::getInstance().getHouseEnd(); ++it){
		House* house = it->second;

		stmt->setInt(1, house->getHouseId() );
		stmt->setInt(2, house->getHouseOwner() );
		stmt->setInt(3, house->getPaidUntil() );
		stmt->setInt(4, house->getPayRentWarnings() );

		if(!stmt->execute()) {
			db->freeStatement(stmt);
			db->rollback();
			return false;
		}
	}

	db->freeStatement(stmt);

	stmt = db->prepareStatement("INSERT INTO `house_lists` (`house_id`, `listid`, `list`) VALUES (?, ?, ?)");

	for(HouseMap::iterator it = Houses::getInstance().getHouseBegin(); it != Houses::getInstance().getHouseEnd(); ++it){
		bool save_lists = false;
		House* house = it->second;

		std::string listText;
		if(house->getAccessList(GUEST_LIST, listText) && listText != ""){
			stmt->setInt(1, house->getHouseId() );
			stmt->setInt(2, GUEST_LIST);
			stmt->setString(3, listText);

			if(!stmt->execute()) {
				db->freeStatement(stmt);
				db->rollback();
				return false;
			}
		}
		if(house->getAccessList(SUBOWNER_LIST, listText) && listText != ""){
			stmt->setInt(1, house->getHouseId() );
			stmt->setInt(2, SUBOWNER_LIST);
			stmt->setString(3, listText);

			if(!stmt->execute()) {
				db->freeStatement(stmt);
				db->rollback();
				return false;
			}
		}

		for(HouseDoorList::iterator it = house->getHouseDoorBegin(); it != house->getHouseDoorEnd(); ++it){
			const Door* door = *it;
			if(door->getAccessList(listText) && listText != ""){
				stmt->setInt(1, house->getHouseId() );
				stmt->setInt(2, door->getDoorId() );
				stmt->setString(3, listText);

				if(!stmt->execute()) {
					db->freeStatement(stmt);
					db->rollback();
					return false;
				}
			}
		}
	}

	db->freeStatement(stmt);

	return db->commit();
}
