//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
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

#ifndef __OTSERV_LUA_MANAGER_H__
#define __OTSERV_LUA_MANAGER_H__

#include <string>
#include <queue>
#include "boost_common.h"

extern "C"
{
	#include <lua.h>
	#include <lauxlib.h>
	#include <lualib.h>
}

class Player;
class Creature;
class Thing;
class Position;

namespace Script {
	typedef uint64_t ObjectID;
	class Enviroment;
	class Manager;
	class Listener;
	typedef shared_ptr<Listener> Listener_ptr;
	typedef weak_ptr<Listener> Listener_wptr;
	class Event;

	enum ErrorMode {
		ERROR_PASS,
		ERROR_WARN,
		ERROR_THROW,
	};

	class Error : public std::runtime_error {
	public:
		Error(const std::string& msg) : std::runtime_error(msg) {}
	};
}

class LuaStateManager;
class LuaThread;

class LuaState /*abstract*/ {
public:
	LuaState(Script::Enviroment& enviroment);
	~LuaState();

	// Stack manipulation
	// Returns the size of the stack
	int getStackTop();
	// Checks if the size of the stack is between low and high
	bool checkStackSize(int low, int high = -1);
	
	// Removes all elements from the stack
	void clearStack();

	// Move value to the given index
	void insert(int idx);
	// Swap top element with a value on the stack
	void swap(int idx);
	// Get the type of the value as a string
	std::string typeOf(int idx = -1);
	// Duplicates the top value at index
	void duplicate(int idx = -1);

	// Table manipulation
	void newTable();

	// Top value as value, index is position of the target table on the stack
	void setField(int index, const std::string& field_name);
	template<typename T>
	void setField(int index, const std::string& field_name, const T& t) {
		push(t);
		setField((index <= -10000? index : (index < 0? index - 1 : index)), field_name);
	}
	// Pushes value onto the stack
	void getField(int index, const std::string& field_name);

	// Top value as value
	void setGlobal(const std::string& gname) {setField(LUA_GLOBALSINDEX, gname);}
	void setRegistryItem(const std::string& rname) {setField(LUA_REGISTRYINDEX, rname);}

	// Pushes value onto the stack
	void getGlobal(const std::string& gname) {getField(LUA_GLOBALSINDEX, gname);}
	void getRegistryItem(const std::string& rname) {getField(LUA_REGISTRYINDEX, rname);}

	// Check
	bool isNil(int index = -1);
	bool isBoolean(int index = -1);
	bool isNumber(int index = -1); // No way to decide if it's double or int
	bool isString(int index = -1);
	bool isUserdata(int index = -1);
	bool isLuaFunction(int index = -1);
	bool isCFunction(int index = -1);
	bool isFunction(int index = -1);
	bool isThread(int index = -1);
	bool isTable(int index = -1);
	// Pop
	void pop(int n = 1);
	bool popBoolean();
	int32_t popInteger();
	uint32_t popUnsignedInteger();
	double popFloat();
	std::string popString();
	Position popPosition(Script::ErrorMode mode = Script::ERROR_THROW);
	void* getUserdata();
	// Push
	void pushNil();
	void pushBoolean(bool b);
	void pushInteger(int32_t i);
	void pushUnsignedInteger(uint32_t ui);
	void pushFloat(double d);
	void pushString(const std::string& str);
	void pushUserdata(void* ptr);
	void pushPosition(const Position& pos);

	// Events
	void pushEvent(Script::Event& event);
	void pushCallback(Script::Listener_ptr listener);
	
	// Advanced types
	// Pop
	Thing* popThing(Script::ErrorMode mode = Script::ERROR_THROW);
	Creature* popCreature(Script::ErrorMode mode = Script::ERROR_THROW);
	Player* popPlayer(Script::ErrorMode mode = Script::ERROR_THROW);
	// Push
	void pushThing(Thing* thing);
	

	// Generic
	void push(bool b) {pushBoolean(b);}
	void push(int32_t i) {pushInteger(i);}
	void push(uint32_t ui) {pushUnsignedInteger(ui);}
	void push(double d) {pushFloat(d);}
	void push(const std::string& str) {pushString(str);}
	void push(Thing* thing) {pushThing(thing);}

	// Don't use pushTable on a userdata class and vice-versa (events are table classes, everything else userdata)
	Script::ObjectID* pushClassInstance(const std::string& classname);
	void pushClassTableInstance(const std::string& classname);

	// Might throw depending on the first parameter, second is equivalent to WARN
	void HandleError(Script::ErrorMode mode, const std::string& error);
	void HandleError(const std::string& error);

	// And finally lua functions that are available for scripts
	// They are instanced in script functions.cpp
	// Global
	// - Utility
	int lua_wait();
	// - Register Events
	int lua_registerGenericEvent_OnSay();
	int lua_registerSpecificEvent_OnSay();

	int lua_stopListener();

	// Classes

	// - Event
	int lua_Event_skip();
	int lua_Event_propagate();

	// - Thing
	int lua_Thing_getPosition();
	int lua_Thing_moveToPosition();
	// - - Creature
	int lua_Creature_getOrientation();
	int lua_Creature_getHealth();
	int lua_Creature_getHealthMax();
	// - - Player
	int lua_Player_setStorageValue();
	int lua_Player_getStorageValue();

	// - Game
	int lua_sendMagicEffect();

protected:
	virtual Script::Manager* getManager() = 0;

	// Members
	lua_State* state;
	Script::Enviroment& enviroment;

	friend class LuaThread;
	friend class LuaStateManager;
	friend class Script::Manager;
};

class LuaThread : public LuaState {
public:
	LuaThread(LuaStateManager& manager, const std::string& name);
	~LuaThread();

	// Returns time to sleep, 0 if execution ended
	int32_t run(int args);

	bool ok() const;
protected:
	virtual Script::Manager* getManager();

	LuaStateManager& manager;
	std::string name;
	int thread_state;
};

typedef shared_ptr<LuaThread> LuaThread_ptr;
typedef weak_ptr<LuaThread> LuaThread_wptr;

class LuaStateManager : public LuaState {
public:
	LuaStateManager(Script::Enviroment& enviroment);
	~LuaStateManager();

	bool loadFile(std::string file);

	LuaThread_ptr newThread(const std::string& name);
	void scheduleThread(int32_t schedule, LuaThread_ptr thread);
	void runScheduledThreads();

	struct ThreadSchedule {
		time_t scheduled_time;
		LuaThread_ptr thread;

		bool operator<(const LuaStateManager::ThreadSchedule& rhs) const {
			return scheduled_time < rhs.scheduled_time;
		}
	};
protected:
	typedef std::map<lua_State*, LuaThread_ptr> ThreadMap;
	ThreadMap threads;
	std::priority_queue<ThreadSchedule> queued_threads;

	friend class LuaStateManager;
};

#endif