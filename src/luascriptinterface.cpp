
#ifdef LUACONSOLE

#include <string>
#include <iostream>
#include <sys/stat.h>

#include "defines.h"
#include "graphics.h"
#include "interface.h"
#include "legacy_console.h"
#include "luascriptinterface.h"
#include "powder.h"
#include "powdergraphics.h"
#include "hud.h"

#include "bzip2/bz2wrap.h"
#include "common/Format.h"
#include "common/Platform.h"
#include "common/tpt-rand.h"
#include "game/Authors.h"
#include "game/Brush.h"
#include "game/Menus.h"
#include "game/Request.h"
#include "game/Save.h"
#include "game/Sign.h"
#include "game/Stamps.h"
#include "game/ToolTip.h"
#include "gui/dialogs/ConfirmPrompt.h"
#include "gui/dialogs/ErrorPrompt.h"
#include "gui/dialogs/InfoPrompt.h"
#include "gui/dialogs/TextPrompt.h"
#include "gui/game/PowderToy.h"
#include "graphics/ARGBColour.h"
#include "graphics/Renderer.h"
#include "graphics/VideoBuffer.h"
#include "interface/Engine.h"
#include "lua/LuaButton.h"
#include "lua/LuaCheckbox.h"
#include "lua/LuaLabel.h"
#include "lua/LuaProgressBar.h"
#include "lua/LuaSDLKeys.h"
#include "lua/LuaSlider.h"
#include "lua/LuaSmartRef.h"
#include "lua/LuaTCPSocket.h"
#include "lua/LuaTextbox.h"
#include "lua/LuaTool.h"
#include "lua/LuaWindow.h"
#include "simulation/Simulation.h"
#include "simulation/WallNumbers.h"
#include "simulation/SnapshotHistory.h"
#include "simulation/ToolNumbers.h"
#include "simulation/Tool.h"
#include "simulation/elements/FIGH.h"
#include "simulation/elements/LIFE.h"
#include "simulation/elements/STKM.h"

#ifdef _MSC_VER
#undef DeleteFile
#endif

static int32_t int32_truncate(double n)
{
	if (n >= 0x1p31)
	{
		n -= 0x1p32;
	}
	return int32_t(n);
}

/*

SIMULATION API

*/

int simulation_signIndex(lua_State *l)
{
	std::string key = tpt_lua_checkString(l, 2);

	//Get Raw Index value for element. Maybe there is a way to get the sign index some other way?
	lua_pushliteral(l, "id");
	lua_rawget(l, 1);
	int id = lua_tointeger(l, lua_gettop(l))-1;

	if (id < 0 || id >= MAXSIGNS)
	{
		luaL_error(l, "Invalid sign ID (stop messing with things): %i", id);
		return 0;
	}
	if (id >= (int)signs.size())
	{
		return lua_pushnil(l), 1;
	}

	if (!key.compare("text"))
		return tpt_lua_pushString(l, signs[id].GetText()), 1;
	else if (!key.compare("displayText"))
		return tpt_lua_pushString(l, signs[id].GetDisplayText(luaSim)), 1;
	else if (!key.compare("linkText"))
		return tpt_lua_pushString(l, signs[id].GetLinkText()), 1;
	else if (!key.compare("justification"))
		return lua_pushnumber(l, signs[id].GetJustification()), 1;
	else if (!key.compare("x"))
		return lua_pushnumber(l, signs[id].GetRealPos().X), 1;
	else if (!key.compare("y"))
		return lua_pushnumber(l, signs[id].GetRealPos().Y), 1;
	else if (!key.compare("screenX"))
	{
		int x, y, w, h;
		signs[id].GetPos(luaSim, x, y, w, h);
		lua_pushnumber(l, x);
		return 1;
	}
	else if (!key.compare("screenY"))
	{
		int x, y, w, h;
		signs[id].GetPos(luaSim, x, y, w, h);
		lua_pushnumber(l, y);
		return 1;
	}
	else if (!key.compare("width"))
	{
		int x, y, w, h;
		signs[id].GetPos(luaSim, x, y, w, h);
		lua_pushnumber(l, w);
		return 1;
	}
	else if (!key.compare("height"))
	{
		int x, y, w, h;
		signs[id].GetPos(luaSim, x, y, w, h);
		lua_pushnumber(l, h);
		return 1;
	}
	else
		return lua_pushnil(l), 1;
}

int simulation_signNewIndex(lua_State *l)
{
	std::string key = tpt_lua_checkString(l, 2);

	//Get Raw Index value for element. Maybe there is a way to get the sign index some other way?
	lua_pushliteral(l, "id");
	lua_rawget(l, 1);
	int id = lua_tointeger(l, lua_gettop(l))-1;

	if (id < 0 || id >= MAXSIGNS)
	{
		luaL_error(l, "Invalid sign ID (stop messing with things)");
		return 0;
	}

	if (!key.compare("text"))
	{
		std::string temp = tpt_lua_checkString(l, 3);
		std::string cleaned = Format::CleanString(temp, false, true, true).substr(0, 45);
		if (!cleaned.empty())
			signs[id].SetText(cleaned);
		else
			luaL_error(l, "Text is empty");
		return 1;
	}
	else if (!key.compare("justification"))
	{
		int ju = luaL_checkinteger(l, 3);
		if (ju >= 0 && ju <= 3)
			return signs[id].SetJustification((Sign::Justification)ju), 1;
		else
			luaL_error(l, "Invalid justification");
		return 0;
	}
	else if (!key.compare("x"))
	{
		int x = luaL_checkinteger(l, 3);
		if (x >= 0 && x < XRES)
			return signs[id].SetPos(Point(x, signs[id].GetRealPos().Y)), 1;
		else
			luaL_error(l, "Invalid X coordinate");
		return 0;
	}
	else if (!key.compare("y"))
	{
		int y = luaL_checkinteger(l, 3);
		if (y >= 0 && y < YRES)
			return signs[id].SetPos(Point(signs[id].GetRealPos().X, y)), 1;
		else
			luaL_error(l, "Invalid Y coordinate");
		return 0;
	}
	else if (!key.compare("displayText") || !key.compare("linkText")  || !key.compare("screenX") || !key.compare("screenY") || !key.compare("width") || !key.compare("height"))
	{
		luaL_error(l, "That property can't be directly set");
	}
	return 0;
}

// Creates a new sign at the first open index
int simulation_newsign(lua_State *l)
{
	if (signs.size() >= MAXSIGNS)
	{
		lua_pushnumber(l, -1);
		return 1;
	}
	std::string temp = tpt_lua_checkString(l, 1);
	int x = luaL_checkinteger(l, 2);
	int y = luaL_checkinteger(l, 3);
	int ju = luaL_optinteger(l, 4, 1);
	if (ju < 0 || ju > 3)
		return luaL_error(l, "Invalid justification");
	if (x < 0 || x >= XRES)
		return luaL_error(l, "Invalid X coordinate");
	if (y < 0 || y >= YRES)
		return luaL_error(l, "Invalid Y coordinate");

	std::string cleaned = Format::CleanString(temp, false, true, true).substr(0, 45);
	signs.push_back(Sign(cleaned, x, y, (Sign::Justification)ju));
	lua_pushnumber(l, signs.size());
	return 1;
}

// Deletes a sign
int simulation_deletesign(lua_State *l)
{
	int signID = luaL_checkinteger(l, 1);
	if (signID <= 0 || signID > (int)signs.size())
		return luaL_error(l, "Sign doesn't exist");

	signs.erase(signs.begin()+signID-1);
	return 1;
}

void initSimulationAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg simulationAPIMethods [] = {
		{"partNeighbors", simulation_partNeighbors},
		{"partChangeType", simulation_partChangeType},
		{"partCreate", simulation_partCreate},
		{"partID", simulation_partID},
		{"partProperty", simulation_partProperty},
		{"partPosition", simulation_partPosition},
		{"partKill", simulation_partKill},
		{"partExists", simulation_partExists},
		{"pressure", simulation_pressure},
		{"velocityX", simulation_velocityX},
		{"velocityY", simulation_velocityY},
		{"ambientHeat", simulation_ambientHeat},
		{"gravityMass", simulation_gravityMass},
		{"wallMap", simulation_wallMap},
		{"elecMap", simulation_elecMap},
		{"fanVelocityX", simulation_fanVelocityX},
		{"fanVelocityY", simulation_fanVelocityY},
		{"ambientHeatSim", simulation_ambientHeatSim},
		{"heatSim", simulation_heatSim},
		{"newtonianGravity", simulation_newtonianGravity},
		{"createParts", simulation_createParts},
		{"createLine", simulation_createLine},
		{"createBox", simulation_createBox},
		{"floodParts", simulation_floodParts},
		{"createWalls", simulation_createWalls},
		{"createWallLine", simulation_createWallLine},
		{"createWallBox", simulation_createWallBox},
		{"floodWalls", simulation_floodWalls},
		{"toolBrush", simulation_toolBrush},
		{"toolLine", simulation_toolLine},
		{"toolBox", simulation_toolBox},
		{"decoBrush", simulation_decoBrush},
		{"decoLine", simulation_decoLine},
		{"decoBox", simulation_decoBox},
		{"floodDeco", simulation_floodDeco},
		{"decoColor", simulation_decoColor},
		{"clearSim", simulation_clearSim},
		{"clearRect", simulation_clearRect},
		{"resetTemp", simulation_resetTemp},
		{"resetPressure", simulation_resetPressure},
		{"saveStamp", simulation_saveStamp},
		{"loadStamp", simulation_loadStamp},
		{"deleteStamp", simulation_deleteStamp},
		{"listStamps", simulation_listStamps},
		{"loadSave", simulation_loadSave},
		{"reloadSave", simulation_reloadSave},
		{"getSaveID", simulation_getSaveID},
		{"adjustCoords", simulation_adjustCoords},
		{"prettyPowders", simulation_prettyPowders},
		{"gravityGrid", simulation_gravityGrid},
		{"edgeMode", simulation_edgeMode},
		{"gravityMode", simulation_gravityMode},
		{"customGravity", simulation_customGravity},
		{"airMode", simulation_airMode},
		{"waterEqualization", simulation_waterEqualization},
		{"ambientAirTemp", simulation_ambientAirTemp},
		{"edgePressure", simulation_edgePressure},
		{"edgeVelocity", simulation_edgeVelocity},
		{"vorticityCoeff", simulation_vorticityCoeff},
		{"convectionMode", simulation_convectionMode},
		{"elementCount", simulation_elementCount},
		{"canMove", simulation_canMove},
		{"parts", simulation_parts},
		{"brush", simulation_brush},
		{"pmap", simulation_pmap},
		{"photons", simulation_photons},
		{"neighbors", simulation_neighbours},
		{"frameRender", simulation_framerender},
		{"golSpeedRatio", simulation_gspeed},
		{"takeSnapshot", simulation_takeSnapshot},
		{"historyRestore", simulation_historyRestore},
		{"historyForward", simulation_historyForward},
		{"replaceModeFlags", simulation_replaceModeFlags},
		{"listCustomGol", simulation_listCustomGol},
		{"listDefaultGol", simulation_listDefaultGol},
		{"addCustomGol", simulation_addCustomGol},
		{"removeCustomGol", simulation_removeCustomGol},
		{"lastUpdatedID", simulation_lastUpdatedID},
		{"updateUpTo", simulation_updateUpTo},
		{"temperatureScale", simulation_temperatureScale},
		{"randomSeed", simulation_randomseed},
		{"hash", simulation_hash},
		{"ensureDeterminism", simulation_ensureDeterminism},
		{"paused", simulation_paused},
		{"partCount", simulation_partCount},
		{"decoSpace", simulation_decoSpace},
		{"gravityField", simulation_gravityField},
		{"resetGravityField", simulation_resetGravityField},
		{"resetSpark", simulation_resetSpark},
		{"resetVelocity", simulation_resetVelocity},
		{"stickman", simulation_stickman},
		{NULL, NULL}
	};
	luaL_register(l, "simulation", simulationAPIMethods);

	//Sim shortcut
	lua_getglobal(l, "simulation");
	lua_setglobal(l, "sim");

	//Static values
	SETCONST(l, CELL);
	SETCONST(l, XCELLS);
	SETCONST(l, YCELLS);
	SETCONST(l, NCELL);
	SETCONST(l, XRES);
	SETCONST(l, YRES);
	SETCONST(l, XCNTR);
	SETCONST(l, YCNTR);
	SETCONSTAS(l, NPART, "MAX_PARTS");
	SETCONST(l, NT);
	SETCONST(l, ST);
	SETCONST(l, ITH);
	SETCONST(l, ITL);
	SETCONSTF(l, IPH);
	SETCONSTF(l, IPL);
	SETCONST(l, PT_NUM);
	SETCONST(l, R_TEMP);
	SETCONST(l, MAX_TEMP);
	SETCONST(l, MIN_TEMP);
	SETCONSTF(l, MAX_PRESSURE);
	SETCONSTF(l, MIN_PRESSURE);
	SETCONST(l, ISTP);
	SETCONSTF(l, CFDS);
	SETCONSTF(l, MAX_VELOCITY);

	SETCONST(l, TOOL_HEAT);
	SETCONST(l, TOOL_COOL);
	SETCONST(l, TOOL_AIR);
	SETCONST(l, TOOL_VAC);
	SETCONST(l, TOOL_PGRV);
	SETCONST(l, TOOL_NGRV);
	SETCONST(l, TOOL_MIX);
	SETCONST(l, TOOL_CYCL);
	SETCONST(l, TOOL_WIND);
	SETCONST(l, TOOL_PROP);
	SETCONST(l, TOOL_SIGN);

	SETCONST(l, DECO_DRAW);
	SETCONST(l, DECO_CLEAR);
	SETCONST(l, DECO_ADD);
	SETCONST(l, DECO_SUBTRACT);
	SETCONST(l, DECO_MULTIPLY);
	SETCONST(l, DECO_DIVIDE);
	SETCONST(l, DECO_LIGHTEN);
	SETCONST(l, DECO_DARKEN);
	SETCONST(l, DECO_SMUDGE);

	SETCONST(l, FLAG_STAGNANT);
	SETCONST(l, FLAG_SKIPMOVE);
	SETCONST(l, FLAG_PHOTDECO);
#ifndef NOMOD
	SETCONST(l, FLAG_EXPLODE);
	SETCONST(l, FLAG_DISAPPEAR);
#endif

	SETCONST(l, PMAPBITS);
	SETCONST(l, PMAPMASK);

	SETCONSTAS(l, CIRCLE_BRUSH, "BRUSH_CIRCLE");
	SETCONSTAS(l, SQUARE_BRUSH, "BRUSH_SQUARE");
	SETCONSTAS(l, TRI_BRUSH, "BRUSH_TRIANGLE");
	SETCONST(l, NUM_DEFAULTBRUSHES);
	SETCONSTAS(l, NUM_DEFAULTBRUSHES, "BRUSH_NUM");

	SETCONST(l, EDGE_VOID);
	SETCONST(l, EDGE_SOLID);
	SETCONST(l, EDGE_LOOP);
	SETCONST(l, NUM_EDGEMODES);

	SETCONST(l, AIR_ON);
	SETCONST(l, AIR_PRESSUREOFF);
	SETCONST(l, AIR_VELOCITYOFF);
	SETCONST(l, AIR_OFF);
	SETCONST(l, AIR_NOUPDATE);
	SETCONST(l, NUM_AIRMODES);

	SETCONST(l, AIRC_NONE);
	SETCONST(l, AIRC_LEGACY);
	SETCONST(l, AIRC_BOUSSINESQ);
	SETCONST(l, NUM_CONVMODES);

	SETCONST(l, GRAV_VERTICAL);
	SETCONST(l, GRAV_OFF);
	SETCONST(l, GRAV_RADIAL);
	SETCONST(l, GRAV_CUSTOM);
	SETCONST(l, NUM_GRAVMODES);

	SETCONST(l, DECOSPACE_SRGB);
	SETCONST(l, DECOSPACE_LINEAR);
	SETCONST(l, DECOSPACE_GAMMA22);
	SETCONST(l, DECOSPACE_GAMMA18);
	SETCONST(l, NUM_DECOSPACES);

	SETCONSTAS(l, 0, "TEMPSCALE_KELVIN");
	SETCONSTAS(l, 1, "TEMPSCALE_CELSIUS");
	SETCONSTAS(l, 2, "TEMPSCALE_FAHRENHEIT");
	SETCONSTAS(l, 3, "NUM_TEMPSCALES");

	SETCONSTAS(l, 0, "CANMOVE_BOUNCE");
	SETCONSTAS(l, 1, "CANMOVE_SWAP");
	SETCONSTAS(l, 2, "CANMOVE_ENTER");
	SETCONSTAS(l, 3, "CANMOVE_BUILTIN");
	SETCONSTAS(l, 4, "NUM_CANMOVEMODES");

	lua_newtable(l);
	for (int i = 0; i < WALLCOUNT; i++)
	{
		tpt_lua_pushString(l, wallTypes[i].identifier);
		lua_pushinteger(l, i);
		lua_settable(l, -3);
		lua_pushinteger(l, i);
		tpt_lua_pushString(l, wallTypes[i].identifier);
		lua_settable(l, -3);
	}
	lua_setfield(l, -2, "walls");
	SETCONSTAS(l, WALLCOUNT, "NUM_WALLS");

	//Declare FIELD_BLAH constants
	int particlePropertiesCount = 0;
	for (auto &prop : particle::GetProperties())
	{
		lua_pushinteger(l, particlePropertiesCount++);
		lua_setfield(l, -2, ("FIELD_" + Format::ToUpper(prop.Name)).c_str());
	}
	for (auto &alias : particle::GetPropertyAliases())
	{
		lua_getfield(l, -1, ("FIELD_" + Format::ToUpper(alias.to)).c_str());
		lua_setfield(l, -2, ("FIELD_" + Format::ToUpper(alias.from)).c_str());
	}

	lua_newtable(l);
	for (int i = 1; i <= MAXSIGNS; i++)
	{
		lua_newtable(l);
		lua_pushinteger(l, i); //set "id" to table index
		lua_setfield(l, -2, "id");
		lua_newtable(l);
		lua_pushcfunction(l, simulation_signIndex);
		lua_setfield(l, -2, "__index");
		lua_pushcfunction(l, simulation_signNewIndex);
		lua_setfield(l, -2, "__newindex");
		lua_setmetatable(l, -2);
		lua_pushinteger(l, i); //table index
		lua_insert(l, -2); //swap k and v
		lua_settable(l, -3); //set metatable to signs[i]
	}
	lua_pushcfunction(l, simulation_newsign);
	lua_setfield(l, -2, "new");
	lua_pushcfunction(l, simulation_deletesign);
	lua_setfield(l, -2, "delete");
	SETCONSTAS(l, Sign::Left,            "JUSTMODE_LEFT"),
	SETCONSTAS(l, Sign::Middle,          "JUSTMODE_MIDDLE"),
	SETCONSTAS(l, Sign::Right,           "JUSTMODE_RIGHT"),
	SETCONSTAS(l, Sign::NoJustification, "JUSTMODE_NONE" ),
	SETCONSTAS(l, Sign::Max,             "NUM_JUSTMODES"),
	SETCONSTAS(l, MAXSIGNS,              "MAX_SIGNS" ),
	lua_setfield(l, -2, "signs");

	for (auto moving = 0; moving < PT_NUM; ++moving)
	{
		for (auto into = 0; into < PT_NUM; ++into)
		{
			custom_can_move[moving][into] = 0;
		}
	}
}

int simulation_partNeighbors(lua_State * l)
{
	int id = 0;
	lua_newtable(l);
	int x = lua_tointeger(l, 1), y = lua_tointeger(l, 2), r = lua_tointeger(l, 3), rx, ry, n;
	// This is one more than the number of arguments because a table has just been pushed onto the stack with lua_newtable(l);
	if(lua_gettop(l) == 5)
	{
		int t = lua_tointeger(l, 4);
		for (rx = -r; rx <= r; rx++)
			for (ry = -r; ry <= r; ry++)
				if (x+rx >= 0 && y+ry >= 0 && x+rx < XRES && y+ry < YRES && (rx || ry))
				{
					n = pmap[y+ry][x+rx];
					if (!n || TYP(n) != t)
						n = photons[y+ry][x+rx];
					if (n && TYP(n) == t)
					{
						lua_pushinteger(l, ID(n));
						lua_rawseti(l, -2, id++);
					}
				}

	}
	else
	{
		for (rx = -r; rx <= r; rx++)
			for (ry = -r; ry <= r; ry++)
				if (x+rx >= 0 && y+ry >= 0 && x+rx < XRES && y+ry < YRES && (rx || ry))
				{
					n = pmap[y+ry][x+rx];
					if (!n)
						n = photons[y+ry][x+rx];
					if (n)
					{
						lua_pushinteger(l, ID(n));
						lua_rawseti(l, -2, id++);
					}
				}
	}
	return 1;
}

int simulation_partChangeType(lua_State * l)
{
	int partIndex = lua_tointeger(l, 1);
	if (partIndex < 0 || partIndex >= NPART || !parts[partIndex].type)
		return 0;
	luaSim->part_change_type(partIndex, (int)(parts[partIndex].x+0.5f), (int)(parts[partIndex].y+0.5f), lua_tointeger(l, 2), true);
	return 0;
}

int simulation_partCreate(lua_State * l)
{
	int newID = lua_tointeger(l, 1);
	if(newID >= NPART || newID < -3)
	{
		lua_pushinteger(l, -1);
		return 1;
	}
	if (newID >= 0 && !parts[newID].type)
	{
		lua_pushinteger(l, -1);
		return 1;
	}
	int type = lua_tointeger(l, 4);
	int v = -1;
	if (lua_gettop(l) >= 5)
	{
		v = lua_tointeger(l, 5);
	}
	else if (type&~PMAPMASK)
	{
		v = ID(type);
		type = TYP(type);
	}
	lua_pushinteger(l, luaSim->part_create(newID, lua_tointeger(l, 2), lua_tointeger(l, 3), type, v));
	return 1;
}

int simulation_partID(lua_State * l)
{
	int x = lua_tointeger(l, 1);
	int y = lua_tointeger(l, 2);
	int amalgam; // "an alloy of mercury with another metal that is solid or liquid at room temperature" What?

	if(x < 0 || x >= XRES || y < 0 || y >= YRES)
	{
		lua_pushnil(l);
		return 1;
	}

	amalgam = pmap[y][x];
	if(!amalgam)
		amalgam = photons[y][x];
	if (!amalgam)
		lua_pushnil(l);
	else
		lua_pushinteger(l, ID(amalgam));
	return 1;
}

int simulation_partPosition(lua_State * l)
{
	int particleID = lua_tointeger(l, 1);
	int argCount = lua_gettop(l);
	if (particleID < 0 || particleID >= NPART || !parts[particleID].type)
	{
		if (argCount == 1)
		{
			lua_pushnil(l);
			lua_pushnil(l);
			return 2;
		} else {
			return 0;
		}
	}
	
	if (argCount == 3)
	{
		float x = luaSim->parts[particleID].x;
		float y = luaSim->parts[particleID].y;
		luaSim->Move(particleID, (int)(x + 0.5f), (int)(y + 0.5f), lua_tonumber(l, 2), lua_tonumber(l, 3));

		return 0;
	}
	else
	{
		lua_pushnumber(l, parts[particleID].x);
		lua_pushnumber(l, parts[particleID].y);
		return 2;
	}
}

int simulation_partProperty(lua_State * l)
{
	int argCount = lua_gettop(l);
	int particleID = luaL_checkinteger(l, 1);

	if (particleID < 0 || particleID >= NPART || !parts[particleID].type)
	{
		if (argCount == 3)
		{
			lua_pushnil(l);
			return 1;
		}
		else
		{
			return 0;
		}
	}

	auto &properties = particle::GetProperties();
	auto prop = properties.end();

	//Get field
	if (lua_type(l, 2) == LUA_TNUMBER)
	{
		int fieldID = lua_tointeger(l, 2);
		if (fieldID < 0 || fieldID >= (int)properties.size())
			return luaL_error(l, "Invalid field ID (%d)", fieldID);
		prop = properties.begin() + fieldID;
	}
	else if (lua_type(l, 2) == LUA_TSTRING)
	{
		std::string fieldName = tpt_lua_toString(l, 2);
		for (auto &alias : particle::GetPropertyAliases())
		{
			if (fieldName == alias.from)
			{
				fieldName = alias.to;
			}
		}
		prop = std::find_if(properties.begin(), properties.end(), [&fieldName](StructProperty const &p) {
			return p.Name == fieldName;
		});
		if (prop == properties.end())
			return luaL_error(l, "Unknown field (%s)", fieldName.c_str());
	}
	else
	{
		return luaL_error(l, "Field ID must be an name (string) or identifier (integer)");
	}

	//Calculate memory address of property
	auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->parts[particleID])) + prop->Offset);

	if (argCount == 3)
	{
		LuaSetParticleProperty(l, particleID, *prop, propertyAddress, 3);
		return 0;
	}
	else
	{
		LuaGetProperty(l, *prop, propertyAddress);
		return 1;
	}
}

int simulation_partKill(lua_State * l)
{
	if (lua_gettop(l) == 2)
		luaSim->part_delete(lua_tointeger(l, 1), lua_tointeger(l, 2));
	else
	{
		int i = lua_tointeger(l, 1);
		if (i>=0 && i<NPART)
			luaSim->part_kill(lua_tointeger(l, 1));
	}
	return 0;
}

int simulation_partExists(lua_State* l)
{
	int i = luaL_checkinteger(l, 1);
	lua_pushboolean(l, i >= 0 && i < NPART && luaSim->parts[i].type);
	return 1;
}

template<class Accessor>
struct LuaBlockMapHelper
{
	using ItemType = typename std::remove_reference<decltype(std::declval<Accessor>()(std::declval<Point>()))>::type;
};

template<bool Clamp, class Accessor, class ItemType = typename LuaBlockMapHelper<Accessor>::ItemType>
static int LuaBlockMapImpl(lua_State *L, ItemType minValue, ItemType maxValue, Accessor accessor)
{
	auto pos = Point{ luaL_checkint(L, 1), luaL_checkint(L, 2) };
	if (!luaSim->InBounds(pos.X * CELL, pos.Y * CELL))
	{
		return luaL_error(L, "Coordinates (%i, %i) out of range", pos.X, pos.Y);
	}
	auto argc = lua_gettop(L);
	if (argc == 2)
	{
		if constexpr (std::is_integral_v<ItemType>)
		{
			lua_pushinteger(L, lua_Integer(accessor(pos)));
		}
		else
		{
			lua_pushnumber(L, lua_Number(accessor(pos)));
		}
		return 1;
	}
	auto size = Point{ 1, 1 };
	auto valuePos = 3;
	if (argc > 3)
	{
		size = Point{ luaL_checkint(L, 3), luaL_checkint(L, 4) };
		valuePos = 5;
	}
	ItemType value;
	if constexpr (std::is_integral_v<ItemType>)
	{
		value = ItemType(luaL_checkint(L, valuePos));
	}
	else
	{
		value = ItemType(luaL_checknumber(L, valuePos));
	}
	if constexpr (Clamp)
	{
		if (value > maxValue) value = maxValue;
		if (value < minValue) value = minValue;
	}
	for (int x = pos.X; x < pos.X + size.X; x++)
	{
		for (int y = pos.Y; y < pos.Y + size.Y; y++)
		{
			Point p = Point{x, y};
			accessor(p) = value;
		}
	}
	return 0;
}

template<class Accessor, class ItemType = typename LuaBlockMapHelper<Accessor>::ItemType>
static int LuaBlockMap(lua_State *L, ItemType minValue, ItemType maxValue, Accessor accessor)
{
	return LuaBlockMapImpl<true>(L, minValue, maxValue, accessor);
}

template<class Accessor, class ItemType = typename LuaBlockMapHelper<Accessor>::ItemType>
static int LuaBlockMap(lua_State *L, Accessor accessor)
{
	return LuaBlockMapImpl<false>(L, ItemType(0), ItemType(0), accessor);
}

int simulation_pressure(lua_State *L)
{
	return LuaBlockMap(L, MIN_PRESSURE, MAX_PRESSURE, [](Point p) -> float & {
		return luaSim->air->pv[p.Y][p.X];
	});
}

int simulation_velocityX(lua_State *L)
{
	return LuaBlockMap(L, MIN_PRESSURE, MAX_PRESSURE, [](Point p) -> float & {
		return luaSim->air->vx[p.Y][p.X];
	});
}

int simulation_velocityY(lua_State *L)
{
	return LuaBlockMap(L, MIN_PRESSURE, MAX_PRESSURE, [](Point p) -> float & {
		return luaSim->air->vy[p.Y][p.X];
	});
}

int simulation_ambientHeat(lua_State *L)
{
	return LuaBlockMap(L, MIN_TEMP, MAX_TEMP, [](Point p) -> float & {
		return luaSim->air->hv[p.Y][p.X];
	});
}

int simulation_gravityMass(lua_State *L)
{
	return LuaBlockMap(L, [](Point p) -> float & {
		return luaSim->grav->gravmap[p.Y * XCELLS + p.X];
	});
}

int simulation_wallMap(lua_State *L)
{
	return LuaBlockMap(L, 0, WALLCOUNT - 1, [](Point p) -> unsigned char & {
		return bmap[p.Y][p.X];
	});
}

int simulation_elecMap(lua_State *L)
{
	return LuaBlockMap(L, [](Point p) -> unsigned char & {
		return emap[p.Y][p.X];
	});
}

int simulation_fanVelocityX(lua_State *L)
{
	return LuaBlockMap(L, [](Point p) -> float & {
		return luaSim->air->fvx[p.Y][p.X];
	});
}

int simulation_fanVelocityY(lua_State *L)
{
	return LuaBlockMap(L, [](Point p) -> float & {
		return luaSim->air->fvy[p.Y][p.X];
	});
}

int simulation_ambientHeatSim(lua_State *l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushboolean(l, aheat_enable);
		return 1;
	}
	auto aheatstate = lua_toboolean(l, 1);
	aheat_enable = aheatstate;

	return 0;
}

int simulation_heatSim(lua_State *l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushboolean(l, !legacy_enable);
		return 1;
	}
	auto heatstate = lua_toboolean(l, 1);
	legacy_enable = !heatstate;
	return 0;
}

int simulation_newtonianGravity(lua_State* l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushboolean(l, luaSim->grav->IsEnabled());
		return 1;
	}
	int gravstate = lua_toboolean(l, 1);
	if (gravstate)
		luaSim->grav->StartAsync();
	else
		luaSim->grav->StopAsync();
	return 0;
}

int simulation_createParts(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int rx = luaL_optint(l,3,5);
	int ry = luaL_optint(l,4,5);
	int c = luaL_optint(l,5,((ElementTool*)activeTools[0])->GetID());
	int brush = luaL_optint(l,6,CIRCLE_BRUSH);
	int flags = luaL_optint(l,7,get_brush_flags());
	if (brush < 0 || brush >= NUM_DEFAULTBRUSHES)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	int ret = luaSim->CreateParts(x, y, c, flags, true, tempBrush);
	delete tempBrush;
	lua_pushinteger(l, ret);
	return 1;
}

int simulation_createLine(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int rx = luaL_optint(l,5,5);
	int ry = luaL_optint(l,6,5);
	int c = luaL_optint(l,7,((ElementTool*)activeTools[0])->GetID());
	int brush = luaL_optint(l,8,CIRCLE_BRUSH);
	int flags = luaL_optint(l,9,get_brush_flags());
	if (brush < 0 || brush >= NUM_DEFAULTBRUSHES)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	luaSim->CreateLine(x1, y1, x2, y2, c, flags, tempBrush);
	delete tempBrush;
	return 0;
}

int simulation_createBox(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int c = luaL_optint(l,5,((ElementTool*)activeTools[0])->GetID());
	int flags = luaL_optint(l,6,get_brush_flags());

	luaSim->CreateBox(x1, y1, x2, y2, c, flags);
	return 0;
}

int simulation_floodParts(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int c = luaL_optint(l,3,((ElementTool*)activeTools[0])->GetID());
	int cm = luaL_optint(l,4,-1);
	int flags = luaL_optint(l,5,get_brush_flags());

	if (x < 0 || x >= XRES || y < 0 || y >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);

	int ret = luaSim->FloodParts(x, y, c, cm, flags);
	lua_pushinteger(l, ret);
	return 1;
}

int simulation_createWalls(lua_State * l)
{
	int x = luaL_optint(l,1,-1)/CELL;
	int y = luaL_optint(l,2,-1)/CELL;
	int rx = luaL_optint(l,3,0)/CELL;
	int ry = luaL_optint(l,4,0)/CELL;
	int c = luaL_optint(l,5,WL_WALL);

	if (x < 0 || x >= XRES || y < 0 || y >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);
	if (c < 0 || c >= WALLCOUNT)
		return luaL_error(l, "Unrecognised wall id '%d'", c);

	luaSim->CreateWallBox(x-rx, y-ry, x+rx, y+ry, c);
	lua_pushinteger(l, 1);
	return 1;
}

int simulation_createWallLine(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1)/CELL;
	int y1 = luaL_optint(l,2,-1)/CELL;
	int x2 = luaL_optint(l,3,-1)/CELL;
	int y2 = luaL_optint(l,4,-1)/CELL;
	int rx = luaL_optint(l,5,0)/CELL;
	int ry = luaL_optint(l,6,0)/CELL;
	int c = luaL_optint(l,7,WL_WALL);

	if (x1 < 0 || x2 < 0 || x1 >= XRES || x2 >= XRES || y1 < 0 || y2 < 0 || y1 >= YRES || y2 >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d),(%d,%d)", x1, y1, x2, y2);
	if (c < 0 || c >= WALLCOUNT)
		return luaL_error(l, "Unrecognised wall id '%d'", c);

	luaSim->CreateWallLine(x1, y1, x2, y2, rx, ry, c);
	return 0;
}

int simulation_createWallBox(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1)/CELL;
	int y1 = luaL_optint(l,2,-1)/CELL;
	int x2 = luaL_optint(l,3,-1)/CELL;
	int y2 = luaL_optint(l,4,-1)/CELL;
	int c = luaL_optint(l,5,WL_WALL);

	if (x1 < 0 || x2 < 0 || x1 >= XRES || x2 >= XRES || y1 < 0 || y2 < 0 || y1 >= YRES || y2 >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d),(%d,%d)", x1, y1, x2, y2);
	if (c < 0 || c >= WALLCOUNT)
		return luaL_error(l, "Unrecognised wall id '%d'", c);

	luaSim->CreateWallBox(x1, y1, x2, y2, c);
	return 0;
}

int simulation_floodWalls(lua_State * l)
{
	int x = luaL_optint(l,1,-1)/CELL;
	int y = luaL_optint(l,2,-1)/CELL;
	int c = luaL_optint(l,3,WL_WALL);
	int bm = luaL_optint(l,4,-1);

	if (x < 0 || x >= XRES || y < 0 || y >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);
	if (c < 0 || c >= WALLCOUNT)
		return luaL_error(l, "Unrecognised wall id '%d'", c);
	if (c == WL_STREAM)
	{
		lua_pushinteger(l, 0);
		return 1;
	}

	int ret = luaSim->FloodWalls(x, y, c, bm);
	lua_pushinteger(l, ret);
	return 1;
}

int simulation_toolBrush(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int rx = luaL_optint(l,3,5);
	int ry = luaL_optint(l,4,5);
	int toolIndex = luaL_optint(l,5,-1);
	int brush = luaL_optint(l,6,CIRCLE_BRUSH);
	float strength = (float)luaL_optnumber(l, 7, 1.0f);
	if (brush < 0 || brush >= NUM_DEFAULTBRUSHES)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	Tool *tool = GetToolByIndex(toolIndex);
	if (!tool)
		return luaL_error(l, "Invalid tool id '%d'", toolIndex);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	tool->DrawPoint(luaSim, tempBrush, { x, y }, strength);
	delete tempBrush;

	lua_pushinteger(l, 0);
	return 1;
}

int simulation_toolLine(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int rx = luaL_optint(l,5,5);
	int ry = luaL_optint(l,6,5);
	int toolIndex = luaL_optint(l,7,-1);
	int brush = luaL_optint(l,8,CIRCLE_BRUSH);
	float strength = (float)luaL_optnumber(l, 9, 1.0f);

	if (x1 < 0 || x2 < 0 || x1 >= XRES || x2 >= XRES || y1 < 0 || y2 < 0 || y1 >= YRES || y2 >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d),(%d,%d)", x1, y1, x2, y2);
	if (brush < 0 || brush >= NUM_DEFAULTBRUSHES)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	Tool *tool = GetToolByIndex(toolIndex);
	if (!tool)
		return luaL_error(l, "Invalid tool id '%d'", toolIndex);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	tool->DrawLine(luaSim, tempBrush, { x1, y1 }, { x2, y2 }, true, strength);
	delete tempBrush;

	return 0;
}

int simulation_toolBox(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int toolIndex = luaL_optint(l,5,-1);
	float strength = (float)luaL_optnumber(l, 6, 1.0f);

	int brush = luaL_optint(l,7,CIRCLE_BRUSH);
	int rx = luaL_optint(l,8,0);
	int ry = luaL_optint(l,9,0);

	if (x1 < 0 || x2 < 0 || x1 >= XRES || x2 >= XRES || y1 < 0 || y2 < 0 || y1 >= YRES || y2 >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d),(%d,%d)", x1, y1, x2, y2);

	Tool *tool = GetToolByIndex(toolIndex);
	if (!tool)
		return luaL_error(l, "Invalid tool id '%d'", toolIndex);

	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	tool->DrawRect(luaSim, tempBrush, { x1, y1 }, { x2, y2}, strength);
	delete tempBrush;

	return 0;
}

int simulation_decoBrush(lua_State * l)
{
	int x = luaL_optint(l,1,-1);
	int y = luaL_optint(l,2,-1);
	int rx = luaL_optint(l,3,5);
	int ry = luaL_optint(l,4,5);
	int r = luaL_optint(l,5,255);
	int g = luaL_optint(l,6,255);
	int b = luaL_optint(l,7,255);
	int a = luaL_optint(l,8,255);
	int tool = luaL_optint(l,9,DECO_DRAW);
	int brush = luaL_optint(l,10,CIRCLE_BRUSH);

	if (tool < 0 || tool >= DECOCOUNT)
			return luaL_error(l, "Invalid tool id '%d'", tool);
	if (brush < 0 || brush >= NUM_DEFAULTBRUSHES)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	unsigned int color = COLARGB(a, r, g, b);
	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	luaSim->CreateDecoBrush(x, y, tool, color, tempBrush);
	delete tempBrush;
	return 0;
}

int simulation_decoLine(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,-1);
	int y2 = luaL_optint(l,4,-1);
	int rx = luaL_optint(l,5,5);
	int ry = luaL_optint(l,6,5);
	int r = luaL_optint(l,7,255);
	int g = luaL_optint(l,8,255);
	int b = luaL_optint(l,9,255);
	int a = luaL_optint(l,10,255);
	int tool = luaL_optint(l,11,DECO_DRAW);
	int brush = luaL_optint(l,12,CIRCLE_BRUSH);

	if (x1 < 0 || x2 < 0 || x1 >= XRES || x2 >= XRES || y1 < 0 || y2 < 0 || y1 >= YRES || y2 >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d),(%d,%d)", x1, y1, x2, y2);
	if (tool < 0 || tool >= DECOCOUNT)
			return luaL_error(l, "Invalid tool id '%d'", tool);
	if (brush < 0 || brush >= NUM_DEFAULTBRUSHES)
		return luaL_error(l, "Invalid brush id '%d'", brush);

	unsigned int color = COLARGB(a, r, g, b);
	Brush* tempBrush = new Brush(Point(rx, ry), brush);
	luaSim->CreateDecoLine(x1, y1, x2, y2, tool, color, tempBrush);
	delete tempBrush;
	return 0;
}

int simulation_decoBox(lua_State * l)
{
	int x1 = luaL_optint(l,1,-1);
	int y1 = luaL_optint(l,2,-1);
	int x2 = luaL_optint(l,3,5);
	int y2 = luaL_optint(l,4,5);
	int r = luaL_optint(l,5,255);
	int g = luaL_optint(l,6,255);
	int b = luaL_optint(l,7,255);
	int a = luaL_optint(l,8,255);
	int tool = luaL_optint(l,9,DECO_DRAW);

	if (x1 < 0 || x2 < 0 || x1 >= XRES || x2 >= XRES || y1 < 0 || y2 < 0 || y1 >= YRES || y2 >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d),(%d,%d)", x1, y1, x2, y2);
	if (tool < 0 || tool >= DECOCOUNT)
		return luaL_error(l, "Invalid tool id '%d'", tool);

	unsigned int color = COLARGB(a, r, g, b);
	luaSim->CreateDecoBox(x1, y1, x2, y2, tool, color);
	return 0;
}

int simulation_floodDeco(lua_State * l)
{
	int x = luaL_checkinteger(l, 1);
	int y = luaL_checkinteger(l, 2);
	int r = luaL_checkinteger(l, 3);
	int g = luaL_checkinteger(l, 4);
	int b = luaL_checkinteger(l, 5);
	int a = luaL_checkinteger(l, 6);

	if (x < 0 || x >= XRES || y < 0 || y >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);

	// hilariously broken, intersects with console and all Lua graphics
	pixel rep = vid_buf[x + y * VIDXRES];
	unsigned int col = COLARGB(r, g, b, a);
	luaSim->FloodDeco(vid_buf, x, y, col, PIXCONV(rep));

	return 0;
}

int simulation_decoColor(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, decocolor);
		return 1;
	}
	else if (acount == 1)
		decocolor = (unsigned int)luaL_optnumber(l, 1, 0xFFFF0000);
	else
	{
		int r, g, b, a;
		r = luaL_optint(l, 1, 255);
		g = luaL_optint(l, 2, 255);
		b = luaL_optint(l, 3, 255);
		a = luaL_optint(l, 4, 255);

		if (r < 0) r = 0; else if (r > 255) r = 255;
		if (g < 0) g = 0; else if (g > 255) g = 255;
		if (b < 0) b = 0; else if (b > 255) b = 255;
		if (a < 0) a = 0; else if (a > 255) a = 255;

		decocolor =  COLARGB(a, r, g, b);
	}
	currR = PIXR(decocolor), currG = PIXG(decocolor), currB = PIXB(decocolor), currA = decocolor>>24;
	RGB_to_HSV(currR, currG, currB, &currH, &currS, &currV);
	return 0;
}

int simulation_clearSim(lua_State * l)
{
	NewSim();
	return 0;
}

int simulation_clearRect(lua_State * l)
{
	int x = luaL_checkint(l,1);
	int y = luaL_checkint(l,2);
	int w = luaL_checkint(l,3);
	int h = luaL_checkint(l,4);
	luaSim->ClearArea(x, y, w, h);
	return 0;
}

int simulation_resetTemp(lua_State * l)
{
	bool onlyConductors = luaL_optint(l, 1, 0) ? true : false;
	for (int i = 0; i <= luaSim->parts_lastActiveIndex; i++)
	{
		if (parts[i].type && (!onlyConductors || !luaSim->IsHeatInsulator(parts[i])))
		{
			parts[i].temp = luaSim->elements[parts[i].type].DefaultProperties.temp;
		}
	}
	return 0;
}

int simulation_resetPressure(lua_State * l)
{
	int aCount = lua_gettop(l), width = XRES/CELL, height = YRES/CELL;
	int x1 = abs(luaL_optint(l, 1, 0));
	int y1 = abs(luaL_optint(l, 2, 0));
	if (aCount > 2)
	{
		width = abs(luaL_optint(l, 3, XRES/CELL));
		height = abs(luaL_optint(l, 4, YRES/CELL));
	}
	else if (aCount)
	{
		width = 1;
		height = 1;
	}
	if(x1 > (XRES/CELL)-1)
		x1 = (XRES/CELL)-1;
	if(y1 > (YRES/CELL)-1)
		y1 = (YRES/CELL)-1;
	if(x1+width > (XRES/CELL)-1)
		width = (XRES/CELL)-x1;
	if(y1+height > (YRES/CELL)-1)
		height = (YRES/CELL)-y1;
	for (int nx = x1; nx<x1+width; nx++)
		for (int ny = y1; ny<y1+height; ny++)
		{
			luaSim->air->pv[ny][nx] = luaSim->air->GetEdgePressure();
		}
	return 0;
}

int simulation_saveStamp(lua_State* l)
{
	int x = luaL_optint(l,1,0);
	int y = luaL_optint(l,2,0);
	int w = luaL_optint(l,3,XRES);
	int h = luaL_optint(l,4,YRES);
	int includePressure = luaL_optint(l,5,1);
	std::string name = Stamps::Ref().Generate(luaSim, x, y, w, h, includePressure);
	tpt_lua_pushString(l, name);
	return 1;
}

int simulation_loadStamp(lua_State* l)
{
	Save *save = NULL;
	int x = luaL_optint(l, 2, 0);
	int y = luaL_optint(l, 3, 0);
	bool hflip = lua_toboolean(l, 4);
	int rotation = luaL_optint(l, 5, 0) & 3; // [0, 3] rotations
	int includePressure = luaL_optint(l, 6, 1);

	// Load from 10 char name, or full filename
	if (lua_isstring(l, 1))
	{
		std::string filename = tpt_lua_optString(l, 1, "");
		save = Stamps::Ref().Load(filename, 0);
		if (!save)
		{
			int size;
			char *load_data = (char*)file_load(filename.c_str(), &size);
			if (load_data)
				save = new Save(load_data, size);
			free(load_data);
		}
	}
	if (!save && lua_isnumber(l, 1))
	{
		int i = luaL_optint(l, 1, 0);
		if (i < 0 || i >= (int)Stamps::Ref().GetNumStamps())
			return luaL_error(l, "Invalid stamp ID: %d", i);
		save = Stamps::Ref().Load(i, 0);
	}
	if (!save)
	{
		lua_pushnil(l);
		lua_pushliteral(l, "Failed to read file");
		return 2;
	}

	int quoX = x / CELL * CELL;
	int quoY = y / CELL * CELL;
	int remX = x % CELL;
	int remY = y % CELL;
	if (remX || remY || hflip || rotation)
	{
		Matrix::matrix2d transform = Matrix::m2d_identity;
		Matrix::vector2d translate = { (float)remX, (float)remY };

		if (hflip)
		{
			transform = m2d_multiply_m2d(Matrix::m2d_mirror_x, transform);
		}
		for (auto i = 0; i < rotation; ++i)
		{
			transform = m2d_multiply_m2d(Matrix::m2d_ccw, transform);
		}
		save->Transform(transform, translate);
	}

	int oldPause = sys_pause;
	int pushed = 1;
	try
	{
		auto saveLoadData = luaSim->LoadSave(quoX, quoY, save, 0, includePressure);
		if (saveLoadData.authors.size())
		{
			saveLoadData.authors["type"] = "luastamp";
			MergeStampAuthorInfo(saveLoadData.authors);
		}
		lua_pushinteger(l, 1);
	}
	catch (ParseException & e)
	{
		lua_pushnil(l);
		tpt_lua_pushString(l, e.message);
		pushed = 2;
	}
	delete save;

	// tpt++ doesn't change pause state with this function, so we won't here either
	sys_pause = oldPause;
	return pushed;
}

int simulation_deleteStamp(lua_State* l)
{
	int stampNum = -1;

	if (lua_isstring(l, 1))
	{
		std::string filename = tpt_lua_optString(l, 1, "");
		stampNum = Stamps::Ref().GetStampId(filename);
	}
	if (stampNum == -1 && lua_isnumber(l, 1))
	{
		stampNum = luaL_optint(l, 1, -1);
		if (stampNum < 0 || stampNum >= (int)Stamps::Ref().GetNumStamps())
			return luaL_error(l, "Invalid stamp ID: %d", stampNum);
	}

	if (stampNum < 0)
	{
		lua_pushnumber(l, -1);
		return 1;
	}
	else
	{
		Stamps::Ref().Delete(stampNum);
		return 0;
	}
}

int simulation_listStamps(lua_State *l)
{
	lua_newtable(l);
	unsigned int numStamps = Stamps::Ref().GetNumStamps();
	for (unsigned int i = 0; i < numStamps; i++)
	{
		tpt_lua_pushString(l, Stamps::Ref().GetStamp(i).name);
		lua_rawseti(l, -2, i + 1);
	}
	return 1;
}

int simulation_loadSave(lua_State * l)
{
	int saveID = luaL_optint(l,1,1);
	int instant = luaL_optint(l,2,0);
	int history = luaL_optint(l,3,0); //Exact second a previous save was saved
	char save_id[24], save_date[24];
	if (saveID < 0)
		return luaL_error(l, "Invalid save ID");
	sprintf(save_id, "%i", saveID);
	sprintf(save_date, "%i", history);
	
	if (open_ui(the_game->GetVid()->GetVid(), save_id, save_date, instant))
	{
		if (console_mode)
			Engine::Ref().CloseTop(ui::Programatic);
	}
	return 0;
}

int simulation_reloadSave(lua_State * l)
{
	the_game->ReloadSave();
	return 0;
}

int simulation_getSaveID(lua_State *l)
{
	if (svf_open)
	{
		lua_pushinteger(l, atoi(svf_id));
		lua_pushinteger(l, Format::StringToNumber<int>(svf_version));
		return 2;
	}
	return 0;
}

int simulation_adjustCoords(lua_State * l)
{
	int x = luaL_optint(l,1,0);
	int y = luaL_optint(l,2,0);
	Point cursor = the_game->AdjustCoordinates(Point(x, y));
	lua_pushinteger(l, cursor.X);
	lua_pushinteger(l, cursor.Y);
	return 2;
}

int simulation_prettyPowders(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, pretty_powder);
		return 1;
	}
	pretty_powder = luaL_optint(l, 1, 0);
	return 0;
}

int simulation_gravityGrid(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, drawgrav_enable);
		return 1;
	}
	drawgrav_enable = luaL_optint(l, 1, 0);
	return 0;
}

int simulation_edgeMode(lua_State * l)
{
	int acount = lua_gettop(l);

	// allow fetching the "temp" edge mode
	bool temp = false;
	if (acount > 1)
	{
		luaL_checktype(l, 2, LUA_TBOOLEAN);
		temp = lua_toboolean(l, 2);
	}

	// get edge mode
	if (acount == 0 || lua_isnil(l, 1))
	{
		if (temp)
			lua_pushnumber(l, luaSim->saveEdgeMode);
		else
			lua_pushnumber(l, luaSim->edgeMode);
		return 1;
	}

	// set edge mode
	int edgeMode = (char)luaL_optint(l, 1, EDGE_VOID);
	luaSim->SetEdgeMode(edgeMode);

	return 0;
}

int simulation_gravityMode(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->gravityMode);
		return 1;
	}
	luaSim->gravityMode = luaL_optint(l, 1, GRAV_VERTICAL);
	return 0;
}

int simulation_customGravity(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->customGravityX);
		lua_pushnumber(l, luaSim->customGravityY);
		return 2;
	}
	else if (acount == 1)
	{
		luaSim->customGravityX = 0.0f;
		luaSim->customGravityY = luaL_optnumber(l, 1, 0.0f);
		return 0;
	}
	luaSim->customGravityX = luaL_optnumber(l, 1, 0.0f);
	luaSim->customGravityY = luaL_optnumber(l, 2, 0.0f);
	return 0;
}

int simulation_airMode(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->air->airMode);
		return 1;
	}
	luaSim->air->airMode = luaL_optint(l, 1, AIR_ON);
	return 0;
}

int simulation_waterEqualization(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, water_equal_test);
		return 1;
	}
	water_equal_test = luaL_optint(l, 1, -1);
	return 0;
}

int simulation_ambientAirTemp(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->air->GetAmbientAirTemp());
		return 1;
	}
	float ambientAirTemp = restrict_flt(luaL_optnumber(l, 1, R_TEMP + 273.15f), MIN_TEMP, MAX_TEMP);
	luaSim->air->SetAmbientAirTempPref(ambientAirTemp);
	return 0;
}

int simulation_edgePressure(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->air->GetEdgePressure());
		return 1;
	}
	float edgePressure = restrict_flt(luaL_optnumber(l, 1, 0), MIN_PRESSURE, MAX_PRESSURE);
	luaSim->air->SetEdgePressurePref(edgePressure);
	return 0;
}

int simulation_edgeVelocity(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->air->GetEdgeVelocityX());
		lua_pushnumber(l, luaSim->air->GetEdgeVelocityY());
		return 2;
	}
	float edgeVelocityX = restrict_flt(luaL_optnumber(l, 1, 0), -MAX_VELOCITY, MAX_VELOCITY);
	float edgeVelocityY = restrict_flt(luaL_optnumber(l, 2, 0), -MAX_VELOCITY, MAX_VELOCITY);
	luaSim->air->SetEdgeVelocityPref(edgeVelocityX, edgeVelocityY);
	return 0;
}

int simulation_vorticityCoeff(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->air->GetVorticityCoeff());
		return 1;
	}
	float vorticityCoeff = restrict_flt(luaL_optnumber(l, 1, 0.0f), 0.0f, 1.0f);
	luaSim->air->SetVorticityCoeffPref(vorticityCoeff);
	return 0;
}

int simulation_convectionMode(lua_State* l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, luaSim->air->GetConvectionMode());
		return 1;
	}
	int convMode = luaL_checkint(l, 1);
	if (convMode < 0 || convMode >= NUM_CONVMODES)
	{
		return luaL_error(l, "invalid convection mode");
	}
	luaSim->air->SetConvectionMode(convMode);
	return 0;
}

int simulation_elementCount(lua_State* l)
{
	int element = luaL_checkint(l, 1);
	if (element < 0 || element >= PT_NUM)
		return luaL_error(l, "Invalid element ID (%d)", element);

	lua_pushnumber(l, luaSim->elementCount[element]);
	return 1;
}

int simulation_canMove(lua_State * l)
{
	int movingElement = luaL_checkint(l, 1);
	int destinationElement = luaL_checkint(l, 2);
	if (movingElement < 0 || movingElement >= PT_NUM)
		return luaL_error(l, "Invalid element ID (%d)", movingElement);
	if (destinationElement < 0 || destinationElement >= PT_NUM)
		return luaL_error(l, "Invalid element ID (%d)", destinationElement);
	
	if (lua_gettop(l) < 3)
	{
		lua_pushnumber(l, luaSim->can_move[movingElement][destinationElement]);
		return 1;
	}
	else
	{
		int setting = (unsigned char)luaL_checkint(l, 3) & 0x7F;
		custom_can_move[movingElement][destinationElement] = setting | 0x80;
		luaSim->can_move[movingElement][destinationElement] = setting;
		return 0;
	}
}

int PartsClosure(lua_State * l)
{
	for (int i = lua_tointeger(l, lua_upvalueindex(1)); i <= luaSim->parts_lastActiveIndex; ++i)
	{
		if (luaSim->parts[i].type)
		{
			lua_pushnumber(l, i + 1);
			lua_replace(l, lua_upvalueindex(1));
			lua_pushnumber(l, i);
			return 1;
		}
	}
	return 0;
}

int simulation_parts(lua_State * l)
{
	lua_pushnumber(l, 0); // first value PartsClosure will see = particle 0
	lua_pushcclosure(l, PartsClosure, 1);
	return 1;
}

int BrushClosure(lua_State * l)
{
	// see Simulation::ToolBrush
	int positionX = lua_tointeger(l, lua_upvalueindex(1));
	int positionY = lua_tointeger(l, lua_upvalueindex(2));
	int radiusX = lua_tointeger(l, lua_upvalueindex(3));
	int radiusY = lua_tointeger(l, lua_upvalueindex(4));
	int x = lua_tointeger(l, lua_upvalueindex(5));
	int y = lua_tointeger(l, lua_upvalueindex(6));
	bool *bitmap = (bool *)lua_touserdata(l, lua_upvalueindex(7));


	int yield_x, yield_y;
	while (true)
	{
		if (!(y < radiusY+radiusY+1))
			return 0;
		if (x < radiusX+radiusX+1)
		{
			bool yield_coords = false;
			if (bitmap[(y*(radiusX+radiusX+1))+x] && (positionX+(x-radiusX) >= 0 && positionY+(y-radiusY) >= 0 && positionX+(x-radiusX) < XRES && positionY+(y-radiusY) < YRES))
			{
				yield_coords = true;
				yield_x = positionX+(x-radiusX);
				yield_y = positionY+(y-radiusY);
			}
			x++;
			if (yield_coords)
				break;
		}
		else
		{
			x = 0;
			y++;
		}
	}

	lua_pushnumber(l, x);
	lua_replace(l, lua_upvalueindex(5));
	lua_pushnumber(l, y);
	lua_replace(l, lua_upvalueindex(6));

	lua_pushnumber(l, yield_x);
	lua_pushnumber(l, yield_y);
	return 2;
}

int simulation_brush(lua_State * l)
{
	int argCount = lua_gettop(l);
	int positionX = luaL_checkint(l, 1);
	int positionY = luaL_checkint(l, 2);
	int brushradiusX, brushradiusY;
	if (argCount >= 4)
	{
		brushradiusX = luaL_checkint(l, 3);
		brushradiusY = luaL_checkint(l, 4);
	}
	else
	{
		Point size = currentBrush->GetRadius();
		brushradiusX = size.X;
		brushradiusY = size.Y;
	}
	int brushID = luaL_optint(l, 5, currentBrush->GetShape());

	if (brushID < 0 || brushID >= NUM_DEFAULTBRUSHES)
		return luaL_error(l, "Invalid brush id '%d'", brushID);
	Point tempRadius = currentBrush->GetRadius();
	int tempID = currentBrush->GetShape();
	currentBrush->SetRadius(Point(brushradiusX, brushradiusY));
	currentBrush->SetShape(brushID);

	int radiusX = currentBrush->GetRadius().X, radiusY = currentBrush->GetRadius().Y;
	lua_pushnumber(l, positionX);
	lua_pushnumber(l, positionY);
	lua_pushnumber(l, radiusX);
	lua_pushnumber(l, radiusY);
	lua_pushnumber(l, 0);
	lua_pushnumber(l, 0);
	size_t bitmapSize = (radiusX+radiusX+1) * (radiusY+radiusY+1) * sizeof(unsigned char);
	void *bitmapCopy = lua_newuserdata(l, bitmapSize);
	memcpy(bitmapCopy, currentBrush->GetBitmap(), bitmapSize);
	lua_pushcclosure(l, BrushClosure, 7);

	currentBrush->SetRadius(tempRadius);
	currentBrush->SetShape(tempID);
	return 1;
}

int simulation_pmap(lua_State * l)
{
	int x = luaL_checkint(l, 1);
	int y = luaL_checkint(l, 2);
	if (x < 0 || x >= XRES || y < 0 || y >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);
	int r = pmap[y][x];
	if (!r)
		return 0;
	lua_pushnumber(l, ID(r));
	return 1;
}

int simulation_photons(lua_State * l)
{
	int x = luaL_checkint(l, 1);
	int y = luaL_checkint(l, 2);
	if (x < 0 || x >= XRES || y < 0 || y >= YRES)
		return luaL_error(l, "coordinates out of range (%d,%d)", x, y);
	int r = photons[y][x];
	if (!r)
		return 0;
	lua_pushnumber(l, ID(r));
	return 1;
}

int NeighboursClosure(lua_State *l)
{
	int cx = lua_tointeger(l, lua_upvalueindex(1));
	int cy = lua_tointeger(l, lua_upvalueindex(2));
	int rx = lua_tointeger(l, lua_upvalueindex(3));
	int ry = lua_tointeger(l, lua_upvalueindex(4));
	int t = lua_tointeger(l, lua_upvalueindex(5));
	int x = lua_tointeger(l, lua_upvalueindex(6));
	int y = lua_tointeger(l, lua_upvalueindex(7));
	while (y <= cy + ry)
	{
		int px = x;
		int py = y;
		x += 1;
		if (x > cx + rx)
		{
			x = cx - rx;
			y += 1;
		}
		int r = pmap[py][px];
		if (!(r && (!t || TYP(r) == t))) // * If not [exists and is of the correct type]
		{
			r = 0;
		}
		if (!r)
		{
			r = photons[py][px];
			if (!(r && (!t || TYP(r) == t))) // * If not [exists and is of the correct type]
			{
				r = 0;
			}
		}
		if (cx == px && cy == py)
		{
			r = 0;
		}
		if (r)
		{
			lua_pushnumber(l, x);
			lua_replace(l, lua_upvalueindex(6));
			lua_pushnumber(l, y);
			lua_replace(l, lua_upvalueindex(7));
			lua_pushnumber(l, ID(r));
			lua_pushnumber(l, px);
			lua_pushnumber(l, py);
			return 3;
		}
	}
	return 0;
}

int simulation_neighbours(lua_State * l)
{
	int cx = luaL_checkint(l, 1);
	int cy = luaL_checkint(l, 2);
	int rx = luaL_optint(l, 3, 2);
	int ry = luaL_optint(l, 4, 2);
	int t = luaL_optint(l, 5, PT_NONE);
	if (rx < 0 || ry < 0)
	{
		luaL_error(l, "Invalid radius");
	}
	lua_pushnumber(l, cx);
	lua_pushnumber(l, cy);
	lua_pushnumber(l, rx);
	lua_pushnumber(l, ry);
	lua_pushnumber(l, t);
	lua_pushnumber(l, cx - rx);
	lua_pushnumber(l, cy - ry);
	lua_pushcclosure(l, NeighboursClosure, 7);
	return 1;
}

int simulation_framerender(lua_State * l)
{
	if (lua_gettop(l) == 0)
	{
		lua_pushinteger(l, framerender);
		return 1;
	}
	int frames = luaL_checkinteger(l, 1);
	if (frames < 0)
		return luaL_error(l, "Can't simulate a negative number of frames");
	framerender = frames;
	return 0;
}

int simulation_gspeed(lua_State * l)
{
	if (lua_gettop(l) == 0)
	{
		lua_pushinteger(l, static_cast<LIFE_ElementDataContainer&>(*luaSim->elementData[PT_LIFE]).golSpeed);
		return 1;
	}
	int gspeed = luaL_checkinteger(l, 1);
	if (gspeed < 1)
		return luaL_error(l, "GSPEED must be at least 1");
	static_cast<LIFE_ElementDataContainer&>(*luaSim->elementData[PT_LIFE]).golSpeed = gspeed;
	return 0;
}

int simulation_takeSnapshot(lua_State * l)
{
	SnapshotHistory::TakeSnapshot(luaSim);
	return 0;
}

int simulation_historyRestore(lua_State *l)
{
	bool successful = SnapshotHistory::HistoryRestore(luaSim);
	lua_pushboolean(l, successful);
	return 1;
}

int simulation_historyForward(lua_State *l)
{
	bool successful = SnapshotHistory::HistoryForward(luaSim);
	lua_pushboolean(l, successful);
	return 1;
}

int simulation_replaceModeFlags(lua_State *l)
{
	if (lua_gettop(l) == 0)
	{
		lua_pushinteger(l, get_brush_flags());
		return 1;
	}
	unsigned int flags = luaL_checkinteger(l, 1);
	if (flags & ~0x3)
		return luaL_error(l, "Invalid flags");
	if ((flags & 0x1) && (flags & 0x2))
		return luaL_error(l, "Cannot set replace mode and specific delete at the same time");
	REPLACE_MODE = flags & 0x1 ? true : false;
	SPECIFIC_DELETE = flags & 0x2 ? true : false;
	return 0;
}

int simulation_listCustomGol(lua_State *l)
{
	int i = 0;
	lua_newtable(l);
	for (auto &cgol : static_cast<LIFE_ElementDataContainer&>(*globalSim->elementData[PT_LIFE]).GetCustomGOL())
	{
		lua_newtable(l);
		tpt_lua_pushString(l, cgol.nameString);
		lua_setfield(l, -2, "name");
		tpt_lua_pushString(l, cgol.ruleString);
		lua_setfield(l, -2, "rulestr");
		lua_pushnumber(l, cgol.rule);
		lua_setfield(l, -2, "rule");
		lua_pushnumber(l, cgol.color1);
		lua_setfield(l, -2, "color1");
		lua_pushnumber(l, cgol.color2);
		lua_setfield(l, -2, "color2");
		lua_rawseti(l, -2, ++i);
	}
	return 1;
}

int simulation_listDefaultGol(lua_State *l)
{
	int i = 0;
	lua_newtable(l);
	for (auto &gol : builtinGol)
	{
		lua_newtable(l);
		tpt_lua_pushString(l, gol.name);
		lua_setfield(l, -2, "name");
		tpt_lua_pushString(l, SerialiseGOLRule(gol.ruleset));
		lua_setfield(l, -2, "rulestr");
		lua_pushnumber(l, gol.ruleset);
		lua_setfield(l, -2, "rule");
		lua_pushnumber(l, COLMODALPHA(gol.color, 0));
		lua_setfield(l, -2, "color1");
		lua_pushnumber(l, COLMODALPHA(gol.color2, 0));
		lua_setfield(l, -2, "color2");
		lua_rawseti(l, -2, ++i);
	}
	return 1;
}

int simulation_addCustomGol(lua_State *l)
{
	CustomGOLData cgol;
	if (lua_isnumber(l, 1))
	{
		cgol.rule = luaL_checkinteger(l, 1);
		cgol.ruleString = SerialiseGOLRule(cgol.rule);
		cgol.rule = ParseGOLString(cgol.ruleString);
	}
	else
	{
		cgol.ruleString = tpt_lua_checkString(l, 1);
		cgol.rule = ParseGOLString(cgol.ruleString);
	}
	cgol.nameString = tpt_lua_checkString(l, 2);
	cgol.color1 = COLMODALPHA(luaL_checkinteger(l, 3), 0);
	cgol.color2 = COLMODALPHA(luaL_checkinteger(l, 4), 0);

	if (cgol.nameString.empty() || !ValidateGOLName(cgol.nameString))
		return luaL_error(l, "Invalid name provided");
	if (cgol.rule == -1)
		return luaL_error(l, "Invalid rule provided");
	if (static_cast<LIFE_ElementDataContainer&>(*globalSim->elementData[PT_LIFE]).GetCustomGOLByRule(cgol.rule))
		return luaL_error(l, "This Custom GoL rule already exists");

	if (!static_cast<LIFE_ElementDataContainer&>(*globalSim->elementData[PT_LIFE]).AddCustomGOL(cgol))
		return luaL_error(l, "Duplicate name, cannot add");
	FillMenus();
	return 0;
}

int simulation_removeCustomGol(lua_State *l)
{
	std::string ruleString = tpt_lua_checkString(l, 1);
	auto cgol = static_cast<LIFE_ElementDataContainer&>(*globalSim->elementData[PT_LIFE]).GetCustomGOLByName(ruleString);
	if (!cgol)
	{
		lua_pushboolean(l, false);
		return 1;
	}
	static_cast<LIFE_ElementDataContainer&>(*globalSim->elementData[PT_LIFE]).RemoveCustomGOL(cgol->rule);
	FillMenus();
	lua_pushboolean(l, true);
	return 1;
}

int simulation_lastUpdatedID(lua_State *l)
{
	if (luaSim->debug_mostRecentlyUpdated != -1)
	{
		lua_pushinteger(l, luaSim->debug_mostRecentlyUpdated);
	}
	else
	{
		lua_pushnil(l);
	}
	return 1;
}

int simulation_updateUpTo(lua_State *l)
{
	int upTo = NPART - 1;
	if (lua_gettop(l) > 0)
	{
		upTo = luaL_checkinteger(l, 1);
	}
	if (upTo < 0 || upTo >= NPART)
	{
		return luaL_error(l, "ID not in valid range");
	}
	if (upTo < luaSim->debug_currentParticle)
	{
		upTo = NPART - 1;
	}
	if (luaSim->debug_currentParticle == 0)
	{
		framerender = 1;
		luaSim->UpdateBefore();
		framerender = 0;
	}
	luaSim->UpdateParticles(luaSim->debug_currentParticle, upTo);
	if (upTo < NPART - 1)
	{
		luaSim->debug_currentParticle = upTo + 1;
	}
	else
	{
		framerender = 1;
		luaSim->UpdateAfter();
		framerender = 0;
		luaSim->debug_currentParticle = 0;
	}
	return 0;
}

int simulation_temperatureScale(lua_State *l)
{
	if (lua_gettop(l) == 0)
	{
		lua_pushinteger(l, luaSim->temperatureScale);
		return 1;
	}
	int temperatureScale = luaL_checkinteger(l, 1);
	if (temperatureScale < 0 || temperatureScale > 2)
		return luaL_error(l, "Invalid temperature scale");
	luaSim->temperatureScale = temperatureScale;
	return 0;
}

int simulation_randomseed(lua_State * l)
{
	if (lua_gettop(l))
	{
		RNG::Ref().state({
			uint32_t(luaL_checkinteger(l, 1)) | (uint64_t(uint32_t(luaL_checkinteger(l, 2))) << 32),
			uint32_t(luaL_checkinteger(l, 3)) | (uint64_t(uint32_t(luaL_checkinteger(l, 4))) << 32),
		});
		return 0;
	}
	auto s = RNG::Ref().state();
	lua_pushinteger(l,  s[0]        & UINT32_C(0xFFFFFFFF));
	lua_pushinteger(l, (s[0] >> 32) & UINT32_C(0xFFFFFFFF));
	lua_pushinteger(l,  s[1]        & UINT32_C(0xFFFFFFFF));
	lua_pushinteger(l, (s[1] >> 32) & UINT32_C(0xFFFFFFFF));
	return 4;
}

int simulation_hash(lua_State * l)
{
	lua_pushinteger(l, Snapshot::Create(luaSim)->Hash());
	return 1;
}

int simulation_ensureDeterminism(lua_State * l)
{
	if (lua_gettop(l))
	{
		return luaL_error(l, "Determinism not available");
	}
	lua_pushboolean(l, false);
	return 1;
}

int simulation_paused(lua_State* l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushboolean(l, sys_pause);
		return 1;
	}
	auto pausestate = lua_toboolean(l, 1);
	the_game->SetPause(pausestate);
	return 0;
}

int simulation_partCount(lua_State *l)
{
	lua_pushinteger(l, NUM_PARTS);
	return 1;
}

int simulation_decoSpace(lua_State *L)
{
	if (lua_gettop(L) < 1)
	{
		lua_pushnumber(L, luaSim->decoSpace);
		return 1;
	}
	auto index = luaL_checkint(L, 1);
	if (index < 0 || index >= NUM_DECOSPACES)
	{
		return luaL_error(L, "Invalid deco space index %i", index);
	}
	luaSim->decoSpace = index;
	return 0;
}

int simulation_gravityField(lua_State *L)
{
	auto pos = Point{ luaL_checkint(L, 1), luaL_checkint(L, 2) };
	if (!luaSim->InBounds(pos.X, pos.Y))
	{
		return luaL_error(L, "Coordinates (%i, %i) out of range", pos.X, pos.Y);
	}
	lua_pushnumber(L, luaSim->grav->gravx[pos.Y * XCELLS + pos.X]);
	lua_pushnumber(L, luaSim->grav->gravy[pos.Y * XCELLS + pos.X]);
	return 2;
}

int simulation_resetGravityField(lua_State * l)
{
	int x1 = abs(luaL_optint(l, 1, 0));
	int y1 = abs(luaL_optint(l, 2, 0));
	int width = abs(luaL_optint(l, 3, XRES/CELL));
	int height = abs(luaL_optint(l, 4, YRES/CELL));
	if (x1 > (XRES/CELL)-1)
		x1 = (XRES/CELL)-1;
	if (y1 > (YRES/CELL)-1)
		y1 = (YRES/CELL)-1;
	if (x1+width > (XRES/CELL)-1)
		width = (XRES/CELL)-x1;
	if (y1+height > (YRES/CELL)-1)
		height = (YRES/CELL)-y1;
	for (int nx = x1; nx < x1 + width; nx++)
		for (int ny = y1; ny < y1 + height; ny++)
		{
			luaSim->grav->gravx[ny*(XRES/CELL)+nx] = 0;
			luaSim->grav->gravy[ny*(XRES/CELL)+nx] = 0;
			luaSim->grav->gravp[ny*(XRES/CELL)+nx] = 0;
		}
	return 0;
}

int simulation_resetSpark(lua_State * l)
{
	for (int i = 0; i < NPART; i++)
	{
		if (parts[i].type == PT_SPRK)
		{
			if (parts[i].ctype >= 0 && parts[i].ctype < PT_NUM && luaSim->elements[parts[i].ctype].Enabled)
			{
				parts[i].type = parts[i].ctype;
				parts[i].life = parts[i].ctype = 0;
			}
			else
				luaSim->part_kill(i);
		}
		else if (parts[i].type == PT_WIRE)
		{
			parts[i].ctype = parts[i].tmp = 0;
		}
	}
	luaSim->elementData[PT_WIFI]->Simulation_Cleared(globalSim);
	return 0;
}

int simulation_resetVelocity(lua_State * l)
{
	int x1 = abs(luaL_optint(l, 1, 0));
	int y1 = abs(luaL_optint(l, 2, 0));
	int width = abs(luaL_optint(l, 3, XRES/CELL));
	int height = abs(luaL_optint(l, 4, YRES/CELL));
	if (x1 > (XRES/CELL)-1)
		x1 = (XRES/CELL)-1;
	if (y1 > (YRES/CELL)-1)
		y1 = (YRES/CELL)-1;
	if (x1+width > (XRES/CELL)-1)
		width = (XRES/CELL)-x1;
	if (y1+height > (YRES/CELL)-1)
		height = (YRES/CELL)-y1;
	for (int nx = x1; nx < x1 + width; nx++)
		for (int ny = y1; ny < y1 + height; ny++)
		{
			luaSim->air->vx[ny][nx] = 0;
			luaSim->air->vy[ny][nx] = 0;
		}
	return 0;
}

//function added only for tptmp really
int simulation_stickman(lua_State *l)
{
	bool set = lua_gettop(l) > 2 && !lua_isnil(l, 3);
	int num = luaL_checkint(l, 1);
	std::string property = tpt_lua_checkString(l, 2);
	double value = 0, ret = -1;
	int offset = luaL_optint(l, 4, 0);
	if (set)
		value = luaL_checknumber(l, 3);

	if (num < 1 || num > static_cast<FIGH_ElementDataContainer&>(*luaSim->elementData[PT_FIGH]).MaxFighters()+2)
		return luaL_error(l, "invalid stickmen number %d", num);
	Stickman *stick;
	if (num == 1)
		stick = static_cast<STKM_ElementDataContainer&>(*luaSim->elementData[PT_STKM]).GetStickman1();
	else if (num == 2)
		stick = static_cast<STKM_ElementDataContainer&>(*luaSim->elementData[PT_STKM]).GetStickman2();
	else
		stick = static_cast<FIGH_ElementDataContainer&>(*luaSim->elementData[PT_FIGH]).Get((unsigned char)(num-3));

	if (byteStringEqualsLiteral(property, "comm"))
	{
		if (set)
			stick->comm = (char)value;
		else
			ret = stick->comm;
	}
	else if (byteStringEqualsLiteral(property, "pcomm"))
	{
		if (set)
			stick->pcomm = (char)value;
		else
			ret = stick->pcomm;
	}
	else if (byteStringEqualsLiteral(property, "elem"))
	{
		if (set)
			stick->elem = (int)value;
		else
			ret = stick->elem;
	}
	else if (byteStringEqualsLiteral(property, "legs"))
	{
		if (offset >= 0 && offset < 16)
		{
			if (set)
				stick->legs[offset] = (float)value;
			else
				ret = stick->legs[offset];
		}
	}
	else if (byteStringEqualsLiteral(property, "accs"))
	{
		if (offset >= 0 && offset < 8)
		{
			if (set)
				stick->accs[offset] = (float)value;
			else
				ret = stick->accs[offset];
		}
	}
	else if (byteStringEqualsLiteral(property, "spwn"))
	{
		if (set)
			stick->spwn = value ? 1 : 0;
		else
			ret = stick->spwn;
	}
	else if (byteStringEqualsLiteral(property, "frames"))
	{
		if (set)
			stick->frames = (unsigned int)value;
		else
			ret = stick->frames;
	}
	else if (byteStringEqualsLiteral(property, "spawnID"))
	{
		if (set)
			stick->spawnID = (int)value;
		else
			ret = stick->spawnID;
	}
	else if (byteStringEqualsLiteral(property, "rocketBoots"))
	{
		if (set)
			stick->rocketBoots = value ? 1 : 0;
		else
			ret = stick->rocketBoots;
	}

	if (!set)
	{
		lua_pushnumber(l, ret);
		return 1;
	}
	return 0;
}

char custom_can_move[PT_NUM][PT_NUM];
void custom_init_can_move()
{
	luaSim->InitCanMove();
	for (auto moving = 0; moving < PT_NUM; ++moving)
	{
		for (auto into = 0; into < PT_NUM; ++into)
		{
			if (custom_can_move[moving][into] & 0x80)
			{
				luaSim->can_move[moving][into] = custom_can_move[moving][into] & 0x7F;
			}
		}
	}
}

/*

RENDERER API

*/

void initRendererAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg rendererAPIMethods [] = {
		{"renderMode", renderer_renderMode},
		{"displayMode", renderer_displayMode},
		{"colorMode", renderer_colorMode},
		{"decorations", renderer_decorations},
		{"grid", renderer_grid},
		{"debugHud", renderer_debugHud},
		{"hud", renderer_hud},
		{"showBrush", renderer_showBrush},
		{"depth3d", renderer_depth3d},
		{"zoomEnabled", renderer_zoomEnabled},
		{"zoomWindow", renderer_zoomWindowInfo},
		{"zoomScope", renderer_zoomScopeInfo},
		{"fireSize", renderer_fireSize},
		{"useDisplayPreset", renderer_useDisplayPreset},
		{NULL, NULL}
	};
	luaL_register(l, "renderer", rendererAPIMethods);

	//Ren shortcut
	lua_getglobal(l, "renderer");
	lua_setglobal(l, "ren");

	//Static values
	//Particle pixel modes/fire mode/effects
	SETCONST(l, PMODE);
	SETCONST(l, PMODE_NONE);
	SETCONST(l, PMODE_FLAT);
	SETCONST(l, PMODE_BLOB);
	SETCONST(l, PMODE_BLUR);
	SETCONST(l, PMODE_GLOW);
	SETCONST(l, PMODE_SPARK);
	SETCONST(l, PMODE_FLARE);
	SETCONST(l, PMODE_LFLARE);
	SETCONST(l, PMODE_ADD);
	SETCONST(l, PMODE_BLEND);
	SETCONST(l, PSPEC_STICKMAN);
	SETCONST(l, OPTIONS);
	SETCONST(l, NO_DECO);
	SETCONST(l, DECO_FIRE);
	SETCONST(l, FIREMODE);
	SETCONST(l, FIRE_ADD);
	SETCONST(l, FIRE_BLEND);
	SETCONST(l, FIRE_SPARK);
	SETCONST(l, EFFECT);
	SETCONST(l, EFFECT_GRAVIN);
	SETCONST(l, EFFECT_GRAVOUT);
	SETCONST(l, EFFECT_LINES);
	SETCONST(l, EFFECT_DBGLINES);

	//Display/Render/Colour modes
	SETCONST(l, RENDER_EFFE);
	SETCONST(l, RENDER_FIRE);
	SETCONST(l, RENDER_SPRK);
	SETCONST(l, RENDER_GLOW);
	SETCONST(l, RENDER_BLUR);
	SETCONST(l, RENDER_BLOB);
	SETCONST(l, RENDER_BASC);
	SETCONST(l, RENDER_NONE);
	SETCONST(l, COLOR_HEAT);
	SETCONST(l, COLOR_LIFE);
	SETCONST(l, COLOR_GRAD);
	SETCONST(l, COLOR_BASC);
	SETCONST(l, COLOR_DEFAULT);
	SETCONST(l, DISPLAY_AIRC);
	SETCONST(l, DISPLAY_AIRP);
	SETCONST(l, DISPLAY_AIRV);
	SETCONST(l, DISPLAY_AIRH);
	SETCONST(l, DISPLAY_AIRW);
	SETCONST(l, DISPLAY_AIR);
	SETCONST(l, DISPLAY_WARP);
	SETCONST(l, DISPLAY_PERS);
	lua_pushinteger(l, 0);
	lua_setfield(l, -2, "DISPLAY_EFFE");
}

//get/set render modes list
int renderer_renderMode(lua_State * l)
{
	if (lua_gettop(l))
	{
		Renderer::Ref().SetRenderMode(luaL_checkinteger(l, 1));
		return 0;
	}
	lua_pushinteger(l, Renderer::Ref().GetRenderMode());
	return 1;
}

int renderer_displayMode(lua_State * l)
{
	if (lua_gettop(l))
	{
		Renderer::Ref().SetDisplayMode(luaL_checkinteger(l, 1));
		return 0;
	}
	lua_pushinteger(l, Renderer::Ref().GetDisplayMode());
	return 1;
}

int renderer_colorMode(lua_State * l)
{
	int args = lua_gettop(l);
	if(args)
	{
		luaL_checktype(l, 1, LUA_TNUMBER);
		Renderer::Ref().SetColorMode(lua_tointeger(l, 1));
		return 0;
	}
	else
	{
		lua_pushnumber(l, Renderer::Ref().GetColorMode());
		return 1;
	}
}

int renderer_decorations(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushboolean(l, decorations_enable);
		return 1;
	}

	decorations_enable = lua_toboolean(l, 1);
	return 0;
}

int renderer_grid(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, GRID_MODE);
		return 1;
	}
	GRID_MODE = luaL_optint(l, 1, 0);
	return 0;
}

int renderer_hud(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushboolean(l, hud_enable);
		return 1;
	}
	hud_enable = lua_toboolean(l, 1);
	if (!hud_enable)
		UpdateToolTip("", Point(16, 20), INTROTIP, 0);
	return 0;
}

int renderer_debugHud(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushboolean(l, DEBUG_MODE);
		return 1;
	}
	DEBUG_MODE = lua_toboolean(l, 1);
	SetCurrentHud();
	return 0;
}

int renderer_showBrush(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, the_game->GetBrushEnable());
		return 1;
	}
	int brush = luaL_optint(l, 1, -1);
	the_game->SetBrushEnable(brush);
	return 0;
}

int renderer_depth3d(lua_State * l)
{
	return luaL_error(l, "This feature is no longer supported");
}

int renderer_zoomEnabled(lua_State * l)
{
	if (lua_gettop(l) == 0)
	{
		lua_pushboolean(l, the_game->IsZoomEnabled());
		return 1;
	}
	else
	{
		luaL_checktype(l, -1, LUA_TBOOLEAN);
		the_game->SetZoomEnabled(lua_toboolean(l, -1));
		return 0;
	}
}
int renderer_zoomWindowInfo(lua_State * l)
{
	int zoomScopeSize = the_game->GetZoomScopeSize();
	int zoomFactor = the_game->GetZoomWindowFactor();
	if (lua_gettop(l) == 0)
	{
		Point location = the_game->GetZoomWindowPosition();
		lua_pushnumber(l, location.X);
		lua_pushnumber(l, location.Y);
		lua_pushnumber(l, zoomFactor);
		lua_pushnumber(l, zoomScopeSize * zoomFactor);
		return 4;
	}
	int x = luaL_optint(l, 1, 0);
	int y = luaL_optint(l, 2, 0);
	int f = luaL_optint(l, 3, 0);
	if (f <= 0)
		return luaL_error(l, "Zoom factor must be greater than 0");

	// To prevent crash when zoom window is outside screen
	if (x < 0 || y < 0 || zoomScopeSize * f + x > XRES || zoomScopeSize * f + y > YRES)
		return luaL_error(l, "Zoom window outside of bounds");

	the_game->SetZoomWindowPosition(Point(x, y));
	the_game->SetZoomWindowFactor(f);
	return 0;
}
int renderer_zoomScopeInfo(lua_State * l)
{
	if (lua_gettop(l) == 0)
	{
		Point location = the_game->GetZoomScopePosition();
		lua_pushnumber(l, location.X);
		lua_pushnumber(l, location.Y);
		lua_pushnumber(l, the_game->GetZoomScopeSize());
		return 3;
	}
	int x = luaL_optint(l, 1, 0);
	int y = luaL_optint(l, 2, 0);
	int s = luaL_optint(l, 3, 0);
	if (s <= 0)
		return luaL_error(l, "Zoom scope size must be greater than 0");

	// To prevent crash when zoom or scope window is outside screen
	int windowEdgeRight = the_game->GetZoomWindowFactor() * s + the_game->GetZoomWindowPosition().X;
	int windowEdgeBottom = the_game->GetZoomWindowFactor() * s + the_game->GetZoomWindowPosition().Y;
	if (x < 0 || y < 0 || x + s > XRES || y + s > YRES)
		return luaL_error(l, "Zoom scope outside of bounds");
	if (windowEdgeRight > XRES || windowEdgeBottom > YRES)
		return luaL_error(l, "Zoom window outside of bounds");

	the_game->SetZoomScopePosition(Point(x, y));
	the_game->SetZoomScopeSize(s);
	return 0;
}

int renderer_useDisplayPreset(lua_State* l)
{
	int cmode = luaL_optint(l, 1, CM_FIRE);
	// Compat with vanilla preset order, which puts CM_CRACK first
	if (cmode == CM_LIFE)
		cmode = CM_CRACK;
	else if (cmode == CM_CRACK)
		cmode = CM_LIFE;
	if (cmode >= 0 && cmode < CM_COUNT)
		the_game->LoadRenderPreset(cmode);
	else
		return luaL_error(l, "Invalid display mode");
	return 0;
}

int renderer_fireSize(lua_State* l)
{
	if (lua_gettop(l) < 1)
	{
		lua_pushnumber(l, fireIntensity);
		return 1;
	}
	float fireintensity = float(luaL_checknumber(l, 1));
	prepare_alpha(fireintensity);
	return 0;
}

/*

FILESYSTEM API

*/

void initFileSystemAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg fileSystemAPIMethods [] = {
		{"list", fileSystem_list},
		{"exists", fileSystem_exists},
		{"isFile", fileSystem_isFile},
		{"isDirectory", fileSystem_isDirectory},
		{"isLink", fileSystem_isLink},
		{"makeDirectory", fileSystem_makeDirectory},
		{"removeDirectory", fileSystem_removeDirectory},
		{"removeFile", fileSystem_removeFile},
		{"move", fileSystem_move},
		{"copy", fileSystem_copy},
		{NULL, NULL}
	};
	luaL_register(l, "fileSystem", fileSystemAPIMethods);

	//elem shortcut
	lua_getglobal(l, "fileSystem");
	lua_setglobal(l, "fs");
}

int fileSystem_list(lua_State * l)
{
	std::string directoryName = tpt_lua_checkString(l, 1);
	lua_newtable(l);
	int index = 0;
	for (auto &name : Platform::DirectorySearch(directoryName, "", {}))
	{
		if (name != "." && name != "..")
		{
			index += 1;
			tpt_lua_pushString(l, name);
			lua_rawseti(l, -2, index);
		}
	}
		return 1;
}

int fileSystem_exists(lua_State * l)
{
	std::string filename = tpt_lua_checkString(l, 1);

	bool ret = Platform::Stat(filename);
	lua_pushboolean(l, ret);
	return 1;
}

int fileSystem_isFile(lua_State * l)
{
	std::string filename = tpt_lua_checkString(l, 1);

	bool ret = Platform::FileExists(filename);
	lua_pushboolean(l, ret);
	return 1;
}

int fileSystem_isDirectory(lua_State * l)
{
	std::string dirname = tpt_lua_checkString(l, 1);

	bool ret = Platform::DirectoryExists(dirname);
	lua_pushboolean(l, ret);
	return 1;
}

int fileSystem_isLink(lua_State * l)
{
	std::string dirname = tpt_lua_checkString(l, 1);

	bool ret = Platform::IsLink(dirname);
	lua_pushboolean(l, ret);
	return 1;
}

int fileSystem_makeDirectory(lua_State * l)
{
	std::string dirname = tpt_lua_checkString(l, 1);

	bool ret = Platform::MakeDirectory(dirname);
	lua_pushboolean(l, ret);
	return 1;
}

int fileSystem_removeDirectory(lua_State * l)
{
	std::string directory = tpt_lua_checkString(l, 1);

	bool ret = Platform::DeleteDirectory(directory);
	lua_pushboolean(l, ret);
	return 1;
}

int fileSystem_removeFile(lua_State * l)
{
	std::string filename = tpt_lua_checkString(l, 1);

	bool ret = Platform::DeleteFile(filename);
	lua_pushboolean(l, ret);
	return 1;
}

int fileSystem_move(lua_State * l)
{
	std::string filename = tpt_lua_checkString(l, 1);
	std::string newFilename = tpt_lua_checkString(l, 2);
	bool replace = lua_toboolean(l, 3);

	lua_pushboolean(l, Platform::RenameFile(filename, newFilename, replace));
	return 1;
}

int fileSystem_copy(lua_State * l)
{
	std::string filename = tpt_lua_checkString(l, 1);
	std::string newFilename = tpt_lua_checkString(l, 2);
	int ret = 1;

	char buf[BUFSIZ];
	size_t size;

	FILE* source = fopen(filename.c_str(), "rb");
	if (source)
	{
		FILE* dest = fopen(newFilename.c_str(), "wb");
		if (dest)
		{
			while ((size = fread(buf, 1, BUFSIZ, source)))
				fwrite(buf, 1, size, dest);

			fclose(dest);
			ret = 0;
		}
		fclose(source);
	}

	lua_pushboolean(l, ret == 0);
	return 1;
}

std::map<LuaComponent *, LuaSmartRef> grabbed_components;
void initInterfaceAPI(lua_State * l)
{
	struct luaL_Reg interfaceAPIMethods [] = {
		{"showWindow", interface_showWindow},
		{"closeWindow", interface_closeWindow},
		{"addComponent", interface_addComponent},
		{"removeComponent", interface_removeComponent},
		{"grabTextInput", interface_grabTextInput},
		{"dropTextInput", interface_dropTextInput},
		{"textInputRect", interface_textInputRect},
		{"beginMessageBox", interface_beginMessageBox},
		{"beginThrowError", interface_beginThrowError},
		{"beginInput", interface_beginInput},
		{"beginConfirm", interface_beginConfirm},
		{"activeMenu", interface_activeMenu},
		{"menuEnabled", interface_menuEnabled},
		{"menuClick", interface_menuClick},
		{"numMenus", interface_numMenus},
		{"perfectCircleBrush", interface_perfectCircleBrush},
		{"console", interface_console},
		{"windowSize", interface_windowSize},
		{"brushID", interface_brushID},
		{"brushRadius", interface_brushRadius},
		{"mousePosition", interface_mousePosition},
		{"activeTool", interface_activeTool},
		{NULL, NULL}
	};
	luaL_register(l, "interface", interfaceAPIMethods);

	//ui shortcut
	lua_getglobal(l, "interface");
	initLuaSDLKeys(l);
	lua_pushinteger(l, PowderToy::mouseUpNormal); lua_setfield(l, -2, "MOUSEUP_NORMAL");
	lua_pushinteger(l, PowderToy::mouseUpBlur); lua_setfield(l, -2, "MOUSEUP_BLUR");
	lua_pushinteger(l, PowderToy::mouseUpDrawEnd); lua_setfield(l, -2, "MOUSEUP_DRAWEND");
	lua_pushinteger(l, 4); lua_setfield(l, -2, "NUM_TOOLINDICES");
	lua_setglobal(l, "ui");

	Luna<LuaWindow>::Register(l);
	Luna<LuaButton>::Register(l);
	Luna<LuaLabel>::Register(l);
	Luna<LuaTextbox>::Register(l);
	Luna<LuaCheckbox>::Register(l);
	Luna<LuaSlider>::Register(l);
	Luna<LuaProgressBar>::Register(l);
}

int interface_showWindow(lua_State * l)
{
	LuaWindow * window = Luna<LuaWindow>::check(l, 1);

	if (window && Engine::Ref().GetTop() != window->GetWindow())
		Engine::Ref().ShowWindow(window->GetWindow());
	return 0;
}

int interface_closeWindow(lua_State * l)
{
	LuaWindow * window = Luna<LuaWindow>::check(l, 1);
	if (window)
		window->GetWindow()->Close(ui::Programatic);
	return 0;
}

int interface_addComponent(lua_State * l)
{
	void *opaque = nullptr;
	LuaComponent *luaComponent = nullptr;
	if ((opaque = Luna<LuaButton>::tryGet(l, 1)))
		luaComponent = Luna<LuaButton>::get(opaque);
	else if ((opaque = Luna<LuaLabel>::tryGet(l, 1)))
		luaComponent = Luna<LuaLabel>::get(opaque);
	else if ((opaque = Luna<LuaTextbox>::tryGet(l, 1)))
		luaComponent = Luna<LuaTextbox>::get(opaque);
	else if ((opaque = Luna<LuaCheckbox>::tryGet(l, 1)))
		luaComponent = Luna<LuaCheckbox>::get(opaque);
	else if ((opaque = Luna<LuaSlider>::tryGet(l, 1)))
		luaComponent = Luna<LuaSlider>::get(opaque);
	else if ((opaque = Luna<LuaProgressBar>::tryGet(l, 1)))
		luaComponent = Luna<LuaProgressBar>::get(opaque);
	else
		luaL_typerror(l, 1, "Component");
	if (luaComponent)
	{
		auto ok = grabbed_components.insert(std::make_pair(luaComponent, LuaSmartRef()));
		if (ok.second)
		{
			auto it = ok.first;
			it->second.Assign(l, 1);
			it->first->owner_ref = it->second;
		}
		the_game->AddComponent(luaComponent->GetComponent());
		luaComponent->GetComponent()->SetSelfManaged();
	}
	return 0;
}

int interface_removeComponent(lua_State * l)
{
	void *opaque = nullptr;
	LuaComponent *luaComponent = nullptr;
	if ((opaque = Luna<LuaButton>::tryGet(l, 1)))
		luaComponent = Luna<LuaButton>::get(opaque);
	else if ((opaque = Luna<LuaLabel>::tryGet(l, 1)))
		luaComponent = Luna<LuaLabel>::get(opaque);
	else if ((opaque = Luna<LuaTextbox>::tryGet(l, 1)))
		luaComponent = Luna<LuaTextbox>::get(opaque);
	else if ((opaque = Luna<LuaCheckbox>::tryGet(l, 1)))
		luaComponent = Luna<LuaCheckbox>::get(opaque);
	else if ((opaque = Luna<LuaSlider>::tryGet(l, 1)))
		luaComponent = Luna<LuaSlider>::get(opaque);
	else if ((opaque = Luna<LuaProgressBar>::tryGet(l, 1)))
		luaComponent = Luna<LuaProgressBar>::get(opaque);
	else
		luaL_typerror(l, 1, "Component");
	if (luaComponent)
	{
		Component *component = luaComponent->GetComponent();
		the_game->RemoveComponent(component);
		auto it = grabbed_components.find(luaComponent);
		if (it != grabbed_components.end())
		{
			it->second.Clear();
			it->first->owner_ref = it->second;
			grabbed_components.erase(it);
		}
	}
	return 0;
}

int textInputRefcount;
int interface_grabTextInput(lua_State * l)
{
	textInputRefcount += 1;
	the_game->SetDoesTextInput(textInputRefcount > 0);
	return 0;
}

int interface_dropTextInput(lua_State * l)
{
	textInputRefcount -= 1;
	the_game->SetDoesTextInput(textInputRefcount > 0);
	return 0;
}

int interface_textInputRect(lua_State * l)
{
	return 0;
}

template<class Type>
struct PickIfTypeHelper;

template<>
struct PickIfTypeHelper<std::string>
{
	static constexpr auto LuaType = LUA_TSTRING;
	static std::string Get(lua_State *l, int index) { return tpt_lua_checkString(l, index); }
};

template<>
struct PickIfTypeHelper<bool>
{
	static constexpr auto LuaType = LUA_TBOOLEAN;
	static bool Get(lua_State *l, int index) { return lua_toboolean(l, index); }
};

template<class Type>
static Type PickIfType(lua_State *l, int index, Type defaultValue)
{
	return lua_type(l, index) == PickIfTypeHelper<Type>::LuaType ? PickIfTypeHelper<Type>::Get(l, index) : defaultValue;
}

int interface_beginMessageBox(lua_State * l)
{
	auto title = PickIfType(l, 1, std::string("Title"));
	auto message = PickIfType(l, 2, std::string("Message"));
	//auto large = PickIfType(l, 3, false); // unused in mod, because info prompts automatically size themselves
	auto cb = std::make_shared<LuaSmartRef>();
	if (lua_gettop(l))
	{
		cb->Assign(l, lua_gettop(l));
	}
	auto prompt = new InfoPrompt(title, message, "OK");
	prompt->SetCallback({ [cb]() {
		lua_State *l = ::l;
		cb->Push(l);
		if (lua_isfunction(l, -1))
		{
			if (tpt_lua_pcall(l, 0, 0, 0))
			{
				luacon_log(luacon_geterror());
			}
		}
		else
		{
			lua_pop(l, 1);
		}
	} });
	Engine::Ref().ShowWindow(prompt);
	return 0;
}

int interface_beginThrowError(lua_State * l)
{
	auto errorMessage = PickIfType(l, 1, std::string("Error text"));
	auto cb = std::make_shared<LuaSmartRef>();
	if (lua_gettop(l))
	{
		cb->Assign(l, lua_gettop(l));
	}
	auto prompt = new ErrorPrompt(errorMessage);
	prompt->SetCallback({ [cb]() {
		lua_State *l = ::l;
		cb->Push(l);
		if (lua_isfunction(l, -1))
		{
			if (tpt_lua_pcall(l, 0, 0, 0))
			{
				luacon_log(luacon_geterror());
			}
		}
		else
		{
			lua_pop(l, 1);
		}
	} });
	Engine::Ref().ShowWindow(prompt);
	return 0;
}

int interface_beginInput(lua_State * l)
{
	auto title = PickIfType(l, 1, std::string("Title"));
	auto prompt = PickIfType(l, 2, std::string("Enter some text:"));
	auto text = PickIfType(l, 3, std::string(""));
	auto shadow = PickIfType(l, 4, std::string(""));
	auto cb = std::make_shared<LuaSmartRef>();
	if (lua_gettop(l))
	{
		cb->Assign(l, lua_gettop(l));
	}
	auto handle = [cb](std::optional<std::string> input) {
		lua_State *l = ::l;
		cb->Push(l);
		if (lua_isfunction(l, -1))
		{
			if (input)
			{
				tpt_lua_pushString(l, *input);
			}
			else
			{
				lua_pushnil(l);
			}
			if (tpt_lua_pcall(l, 1, 0, 0))
			{
				luacon_log(luacon_geterror());
			}
		}
		else
		{
			lua_pop(l, 1);
		}
	};
	auto textPrompt = new TextPrompt(title, prompt, text, shadow);
	textPrompt->SetCallback({ handle });
	Engine::Ref().ShowWindow(textPrompt);

	return 0;
}

int interface_beginConfirm(lua_State * l)
{
	auto title = PickIfType(l, 1, std::string("Title"));
	auto message = PickIfType(l, 2, std::string("Message"));
	auto buttonText = PickIfType(l, 3, std::string("Confirm"));
	auto cb = std::make_shared<LuaSmartRef>();
	if (lua_gettop(l))
	{
		cb->Assign(l, lua_gettop(l));
	}
	auto prompt = new ConfirmPrompt(title, message, buttonText);
	prompt->SetCallback({ [cb](bool wasConfirmed) {
		lua_State *l = ::l;
		cb->Push(l);
		if (lua_isfunction(l, -1))
		{
			lua_pushboolean(l, wasConfirmed);
			if (tpt_lua_pcall(l, 1, 0, 0))
			{
				luacon_log(luacon_geterror());
			}
		}
		else
		{
			lua_pop(l, 1);
		}
	} });
	Engine::Ref().ShowWindow(prompt);
	return 0;
}

int interface_activeMenu(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushnumber(l, active_menu);
		return 1;
	}
	int menuid = luaL_checkint(l, 1);
	if (menuid < SC_TOTAL && menuid >= 0)
		active_menu = menuid;
	else
		return luaL_error(l, "Invalid menu");
	return 0;
}

int interface_menuEnabled(lua_State * l)
{
	int menusection = luaL_checkint(l, 1);
	if (menusection < 0 || menusection >= SC_TOTAL)
		return luaL_error(l, "Invalid menu");
	int acount = lua_gettop(l);
	if (acount == 1)
	{
		lua_pushboolean(l, menuSections[menusection]->enabled);
		return 1;
	}
	luaL_checktype(l, 2, LUA_TBOOLEAN);
	int enabled = lua_toboolean(l, 2);
	menuSections[menusection]->enabled = enabled;
	return 0;
}

int interface_menuClick(lua_State * l)
{
	int menusection = luaL_checkint(l, 1);
	if (menusection < 0 || menusection >= SC_TOTAL)
		return luaL_error(l, "Invalid menu");
	int acount = lua_gettop(l);
	if (acount == 1)
	{
		lua_pushboolean(l, menuSections[menusection]->click);
		return 1;
	}
	luaL_checktype(l, 2, LUA_TBOOLEAN);
	int click = lua_toboolean(l, 2);
	menuSections[menusection]->click = click;
	return 0;
}

int interface_numMenus(lua_State * l)
{
	int acount = lua_gettop(l);
	bool onlyEnabled = true;
	if (acount > 0)
	{
		luaL_checktype(l, 1, LUA_TBOOLEAN);
		onlyEnabled = lua_toboolean(l, 1);
	}
	lua_pushinteger(l, GetNumMenus(onlyEnabled));
	return 1;
}

int interface_perfectCircleBrush(lua_State * l)
{
	if (!lua_gettop(l))
	{
		lua_pushboolean(l, perfectCircleBrush);
		return 1;
	}
	luaL_checktype(l, 1, LUA_TBOOLEAN);
	perfectCircleBrush = lua_toboolean(l, 1);
	return 0;
}

int interface_console(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount == 0)
	{
		lua_pushboolean(l, console_mode);
		return 1;
	}
	bool consolestate = lua_toboolean(l, 1);
	if (consolestate != console_mode)
	{
		// scripts can only run in main window or console window, so just assume console window is on top and close it
		if (console_mode)
			Engine::Ref().CloseTop(ui::Programatic);
		else
			the_game->OpenConsole();
	}
	return 0;
}

int interface_windowSize(lua_State * l)
{
	if (lua_gettop(l) < 1)
	{
		lua_pushinteger(l, Engine::Ref().GetScale());
		lua_pushboolean(l, Engine::Ref().IsFullscreen());
		return 2;
	}
	int scale = luaL_optint(l, 1, 1);
	bool fullscreen = lua_toboolean(l, 2);
	if (scale < 1 || scale > 10)
		scale = 1;
	if (fullscreen != true)
		fullscreen = 0;
	Engine::Ref().SetScale(scale);
	Engine::Ref().SetFullscreen(fullscreen);
	return 0;
}

int interface_brushID(lua_State * l)
{
	if (lua_gettop(l) < 1)
	{
		lua_pushnumber(l, currentBrush->GetShape());
		return 1;
	}
	auto index = luaL_checkint(l, 1);
	if (index < 0 || index >= NUM_DEFAULTBRUSHES)
	{
		return luaL_error(l, "Invalid brush index %i", index);
	}
	currentBrush->SetShape(index);
	return 0;
}

int interface_brushRadius(lua_State * l)
{
	if (lua_gettop(l) < 1)
	{
		auto radius = currentBrush->GetRadius();
		lua_pushnumber(l, radius.X);
		lua_pushnumber(l, radius.Y);
		return 2;
	}
	currentBrush->SetRadius({ luaL_checkint(l, 1), luaL_checkint(l, 2) });
	return 0;
}

int interface_mousePosition(lua_State * l)
{
	auto pos = the_game->GetMousePos();
	lua_pushnumber(l, pos.X);
	lua_pushnumber(l, pos.Y);
	return 2;
}

int interface_activeTool(lua_State * l)
{
	auto index = luaL_checkint(l, 1);
	if (index < 0 || index >= 4)
	{
		return luaL_error(l, "Invalid tool index %i", index);
	}
	// Mod doesn't support separate middle click element, so both are index 3
	if (index == 3)
		index = 2;
	if (lua_gettop(l) < 2)
	{
		tpt_lua_pushString(l, activeTools[index]->GetIdentifier());
		return 1;
	}
	auto identifier = tpt_lua_checkString(l, 2);
	auto *tool = GetToolFromIdentifier(identifier);
	if (!tool || tool->GetType() == INVALID_TOOL)
	{
		return luaL_error(l, "Invalid tool identifier %s", identifier.c_str());
	}
	activeTools[index] = tool;
	tool->Select(index);
	return 0;
}

/*

GRAPHICS API

*/

void initGraphicsAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg graphicsAPIMethods [] = {
		{"textSize", graphics_textSize},
		{"drawText", graphics_drawText},
		{"drawPixel", graphics_drawPixel},
		{"drawLine", graphics_drawLine},
		{"drawRect", graphics_drawRect},
		{"fillRect", graphics_fillRect},
		{"drawCircle", graphics_drawCircle},
		{"fillCircle", graphics_fillCircle},
		{"getColors", graphics_getColors},
		{"getHexColor", graphics_getHexColor},
		{"setClipRect", graphics_setClipRect},
		{"toolTip", graphics_toolTip},
		{NULL, NULL}
	};
	luaL_register(l, "graphics", graphicsAPIMethods);

	//elem shortcut
	lua_getglobal(l, "graphics");
	lua_setglobal(l, "gfx");

	lua_pushinteger(l, XRES+BARSIZE);	lua_setfield(l, -2, "WIDTH");
	lua_pushinteger(l, YRES+MENUSIZE);	lua_setfield(l, -2, "HEIGHT");
}

#include "graphics/VideoBuffer.h"
int graphics_textSize(lua_State * l)
{
	std::string text = tpt_lua_optString(l, 1, "");

	Point size = gfx::VideoBuffer::TextSize(text);
	lua_pushinteger(l, size.X);
	lua_pushinteger(l, size.Y);
	return 2;
}

int graphics_drawText(lua_State * l)
{
	int x, y, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	std::string text = tpt_lua_optString(l, 3, "");
	r = luaL_optint(l, 4, 255);
	g = luaL_optint(l, 5, 255);
	b = luaL_optint(l, 6, 255);
	a = luaL_optint(l, 7, 255);
	
	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	if (eventTrait == eventTraitSimGraphics)
	{
		drawtext(vid_buf, x, y, text.c_str(), r, g, b, a);
	}
	else
	{
		Point adjPos = Engine::Ref().GetTop()->GetPosition();
		Engine::Ref().GetTop()->GetVid()->DrawString(x - adjPos.X, y - adjPos.Y, text, r, g, b, a);
	}
	return 0;
}

int graphics_drawPixel(lua_State *l)
{
	int x = luaL_optint(l, 1, 0);
	int y = luaL_optint(l, 2, 0);
	int r = luaL_optint(l, 3, 255);
	int g = luaL_optint(l, 4, 255);
	int b = luaL_optint(l, 5, 255);
	int a = luaL_optint(l, 6, 255);
	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	if (eventTrait == eventTraitSimGraphics)
	{
		if (x < 0 || y < 0 || x >= VIDXRES || y >= VIDYRES)
			return 0;

		drawpixel(vid_buf, x, y, r, g, b, a);
	}
	else
	{
		auto top = Engine::Ref().GetTop();
		if (x < 0 || y < 0 || x >= top->GetSize().X || y >= top->GetSize().Y)
			return 0;

		Point adjPos = top->GetPosition();
		top->GetVid()->DrawPixel(x - adjPos.X, y - adjPos.Y, r, g, b, a);
	}

	return 0;
}

int graphics_drawLine(lua_State * l)
{
	int x1, y1, x2, y2, r, g, b, a;
	x1 = lua_tointeger(l, 1);
	y1 = lua_tointeger(l, 2);
	x2 = lua_tointeger(l, 3);
	y2 = lua_tointeger(l, 4);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	if (eventTrait == eventTraitSimGraphics)
	{
		blend_line(vid_buf, x1, y1, x2, y2, r, g, b, a);
	}
	else
	{
		Point adjPos = Engine::Ref().GetTop()->GetPosition();
		Engine::Ref().GetTop()->GetVid()->DrawLine(x1 - adjPos.X, y1 - adjPos.Y, x2 - adjPos.X, y2 - adjPos.Y, r, g, b, a);
	}
	return 0;
}

int graphics_drawRect(lua_State * l)
{
	int x, y, w, h, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	w = lua_tointeger(l, 3);
	h = lua_tointeger(l, 4);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	if (eventTrait == eventTraitSimGraphics)
	{
		drawrect(vid_buf, x, y, w - 1, h - 1, r, g, b, a);
	}
	else
	{
		Point adjPos = Engine::Ref().GetTop()->GetPosition();
		Engine::Ref().GetTop()->GetVid()->DrawRect(x - adjPos.X, y - adjPos.Y, w, h, r, g, b, a);
	}
	return 0;
}

int graphics_fillRect(lua_State * l)
{
	int x, y, w, h, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	w = lua_tointeger(l, 3);
	h = lua_tointeger(l, 4);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	if (eventTrait == eventTraitSimGraphics)
	{
		fillrect(vid_buf, x - 1, y - 1, w + 1, h + 1, r, g, b, a);
	}
	else
	{
		Point adjPos = Engine::Ref().GetTop()->GetPosition();
		Engine::Ref().GetTop()->GetVid()->FillRect(x - adjPos.X, y - adjPos.Y, w, h, r, g, b, a);
	}
	return 0;
}

int graphics_drawCircle(lua_State * l)
{
	int x, y, w, h, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	w = lua_tointeger(l, 3);
	h = lua_tointeger(l, 4);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	if (eventTrait == eventTraitSimGraphics)
	{
		drawcircle(vid_buf, x, y, w, h, r, g, b, a);
	}
	else
	{
		Point adjPos = Engine::Ref().GetTop()->GetPosition();
		Engine::Ref().GetTop()->GetVid()->DrawCircle(x - adjPos.X, y - adjPos.Y, w, h, r, g, b, a);
	}
	return 0;
}

int graphics_fillCircle(lua_State * l)
{
	int x, y, w, h, r, g, b, a;
	x = lua_tointeger(l, 1);
	y = lua_tointeger(l, 2);
	w = lua_tointeger(l, 3);
	h = lua_tointeger(l, 4);
	r = luaL_optint(l, 5, 255);
	g = luaL_optint(l, 6, 255);
	b = luaL_optint(l, 7, 255);
	a = luaL_optint(l, 8, 255);

	if (r<0) r = 0;
	else if (r>255) r = 255;
	if (g<0) g = 0;
	else if (g>255) g = 255;
	if (b<0) b = 0;
	else if (b>255) b = 255;
	if (a<0) a = 0;
	else if (a>255) a = 255;

	if (eventTrait == eventTraitSimGraphics)
	{
		fillcircle(vid_buf, x, y, w, h, r, g, b, a);
	}
	else
	{
		Point adjPos = Engine::Ref().GetTop()->GetPosition();
		Engine::Ref().GetTop()->GetVid()->FillCircle(x - adjPos.X, y - adjPos.Y, w, h, r, g, b, a);
	}
	return 0;
}

int graphics_getColors(lua_State * l)
{
	unsigned int color = int32_truncate(lua_tonumber(l, 1));

	int a = color >> 24;
	int r = (color >> 16)&0xFF;
	int g = (color >> 8)&0xFF;
	int b = color&0xFF;

	lua_pushinteger(l, r);
	lua_pushinteger(l, g);
	lua_pushinteger(l, b);
	lua_pushinteger(l, a);
	return 4;
}

int graphics_getHexColor(lua_State * l)
{
	int r = lua_tointeger(l, 1);
	int g = lua_tointeger(l, 2);
	int b = lua_tointeger(l, 3);
	int a = 0;
	if (lua_gettop(l) >= 4)
		a = lua_tointeger(l, 4);
	unsigned int color = (a<<24) + (r<<16) + (g<<8) + b;

	lua_pushinteger(l, color);
	return 1;
}

int graphics_setClipRect(lua_State * l)
{
	int x = luaL_optinteger(l, 1, 0);
	int y = luaL_optinteger(l, 2, 0);
	int w = luaL_optinteger(l, 3, VIDXRES);
	int h = luaL_optinteger(l, 4, VIDYRES);

	if (x < 0 || y < 0 || w < 0 || h < 0)
		return luaL_error(l, "Arguments cannot be negative");
	if (x + w > VIDXRES || y + h > VIDYRES)
		return luaL_error(l, "Size must be within window bounds");

	Point parentWindowPos = Engine::Ref().GetTop()->GetPosition();
	Point parentWindowSize = Engine::Ref().GetTop()->GetSize();
	set_clip_rect(x, y, w, h);
	x -= parentWindowPos.X;
	y -= parentWindowPos.Y;
	Point upperLeft = Point(tpt::max(x - parentWindowPos.X, 0), tpt::max(y - parentWindowPos.Y, 0));
	Point bottomRight = Point(tpt::min(w, parentWindowSize.X), tpt::min(h, parentWindowSize.Y));
	Engine::Ref().GetTop()->GetVid()->SwapClipRect(upperLeft, bottomRight);

	lua_pushinteger(l, upperLeft.X + parentWindowPos.X);
	lua_pushinteger(l, upperLeft.Y + parentWindowPos.Y);
	lua_pushinteger(l, bottomRight.X + parentWindowPos.X);
	lua_pushinteger(l, bottomRight.Y + parentWindowPos.Y);
	return 4;

	return 0;
}

int graphics_toolTip(lua_State *l)
{
	std::string toolTip = tpt_lua_checkString(l, 1);
	int x = luaL_checkinteger(l, 2);
	int y = luaL_checkinteger(l, 3);
	int alpha = luaL_optint(l, 4, 255);
	int ID = luaL_optint(l, 5, LUATIP);

	UpdateToolTip(toolTip, Point(x, y), ID, alpha);
	return 0;
}

/*

ELEMENTS API

*/

void initElementsAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg elementsAPIMethods [] = {
		{"allocate", elements_allocate},
		{"element", elements_element},
		{"property", elements_property},
		{"free", elements_free},
		{"exists", elements_exists},
		{"loadDefault", elements_loadDefault},
		{"getByName", elements_getByName},
		{NULL, NULL}
	};
	luaL_register(l, "elements", elementsAPIMethods);

	//elem shortcut
	lua_getglobal(l, "elements");
	lua_setglobal(l, "elem");

	//Static values
	//Element types/properties/states
	SETCONST(l, TYPE_PART);
	SETCONST(l, TYPE_LIQUID);
	SETCONST(l, TYPE_SOLID);
	SETCONST(l, TYPE_GAS);
	SETCONST(l, TYPE_ENERGY);
	SETCONST(l, PROP_CONDUCTS);
	SETCONST(l, PROP_PHOTPASS);
	SETCONST(l, PROP_NEUTPENETRATE);
	SETCONST(l, PROP_NEUTABSORB);
	SETCONST(l, PROP_NEUTPASS);
	SETCONST(l, PROP_DEADLY);
	SETCONST(l, PROP_HOT_GLOW);
	SETCONST(l, PROP_LIFE);
	SETCONST(l, PROP_RADIOACTIVE);
	SETCONST(l, PROP_LIFE_DEC);
	SETCONST(l, PROP_LIFE_KILL);
	SETCONST(l, PROP_LIFE_KILL_DEC);
	SETCONST(l, PROP_INDESTRUCTIBLE);
	SETCONST(l, PROP_CLONE);
	SETCONST(l, PROP_BREAKABLECLONE);
	SETCONST(l, PROP_POWERED);
	SETCONST(l, PROP_SPARKSETTLE);
	SETCONST(l, PROP_NOAMBHEAT);
	SETCONST(l, PROP_NOCTYPEDRAW);

	SETCONST(l, SC_WALL);
	SETCONST(l, SC_ELEC);
	SETCONST(l, SC_POWERED);
	SETCONST(l, SC_SENSOR);
	SETCONST(l, SC_FORCE);
	SETCONST(l, SC_EXPLOSIVE);
	SETCONST(l, SC_GAS);
	SETCONST(l, SC_LIQUID);
	SETCONST(l, SC_POWDERS);
	SETCONST(l, SC_SOLIDS);
	SETCONST(l, SC_NUCLEAR);
	SETCONST(l, SC_SPECIAL);
	SETCONST(l, SC_LIFE);
	SETCONST(l, SC_TOOL);
	SETCONST(l, SC_DECO);
	SETCONST(l, SC_FAV);
	SETCONST(l, SC_FAV2);
	SETCONST(l, SC_HUD);
	SETCONST(l, SC_OTHER);
	SETCONST(l, SC_SEARCH);
	SETCONST(l, SC_TOTAL);
	SETCONSTAS(l, SC_DECO + 1, "NUM_MENUSECTIONS");

	SETCONST(l, UPDATE_AFTER);
	SETCONST(l, UPDATE_REPLACE);
	SETCONST(l, UPDATE_BEFORE);
	SETCONST(l, NUM_UPDATEMODES);

	//Element identifiers
	for (int i = 0; i < PT_NUM; i++)
	{
		ManageElementIdentifier(l, i, true);
	}
}

void LuaGetProperty(lua_State* l, StructProperty property, intptr_t propertyAddress)
{
	switch (property.Type)
	{
	case StructProperty::TransitionType:
	case StructProperty::ParticleType:
	case StructProperty::Integer:
		lua_pushnumber(l, *((int*)propertyAddress));
		break;
	case StructProperty::UInteger:
		lua_pushnumber(l, *((unsigned int*)propertyAddress));
		break;
	case StructProperty::Float:
		lua_pushnumber(l, *((float*)propertyAddress));
		break;
	case StructProperty::UChar:
		lua_pushnumber(l, *((unsigned char*)propertyAddress));
		break;
	case StructProperty::BString:
	case StructProperty::String:
	{
		tpt_lua_pushString(l, (*((std::string*)propertyAddress)));
		break;
	}
	case StructProperty::Colour:
#if PIXELSIZE == 4
		lua_pushinteger(l, *((unsigned int*)propertyAddress));
#else
		lua_pushinteger(l, *((unsigned short*)propertyAddress));
#endif
		break;
	case StructProperty::Removed:
		lua_pushnil(l);
	}
}

void LuaSetProperty(lua_State* l, StructProperty property, intptr_t propertyAddress, int stackPos)
{
	switch (property.Type)
	{
	case StructProperty::TransitionType:
	case StructProperty::ParticleType:
	case StructProperty::Integer:
		*((int*)propertyAddress) = int32_truncate(luaL_checknumber(l, stackPos));
		break;
	case StructProperty::UInteger:
		*((unsigned int*)propertyAddress) = int32_truncate(luaL_checknumber(l, stackPos));
		break;
	case StructProperty::Float:
		*((float*)propertyAddress) = luaL_checknumber(l, stackPos);
		break;
	case StructProperty::UChar:
		*((unsigned char*)propertyAddress) = int32_truncate(luaL_checknumber(l, stackPos));
		break;
	case StructProperty::BString:
	case StructProperty::String:
		*((std::string*)((unsigned char*)propertyAddress)) = tpt_lua_checkString(l, 3);
		break;
	case StructProperty::Colour:
#if PIXELSIZE == 4
		*((unsigned int*)propertyAddress) = int32_truncate(luaL_checknumber(l, stackPos));
#else
		*((unsigned short*)propertyAddress) = int32_truncate(luaL_checknumber(l, stackPos));
#endif
		break;
	case StructProperty::Removed:
		break;
	}
}

void LuaSetParticleProperty(lua_State* l, int particleID, StructProperty property, intptr_t propertyAddress, int stackPos)
{
	if (property.Name == "type")
	{
		luaSim->part_change_type(particleID, int(luaSim->parts[particleID].x+0.5f), int(luaSim->parts[particleID].y+0.5f), luaL_checkinteger(l, 3), true);
	}
	else if (property.Name == "x" || property.Name == "y")
	{
		float val = luaL_checknumber(l, 3);
		float x = luaSim->parts[particleID].x;
		float y = luaSim->parts[particleID].y;
		float nx = property.Name == "x" ? val : x;
		float ny = property.Name == "y" ? val : y;
		luaSim->Move(particleID, (int)(x + 0.5f), (int)(y + 0.5f), nx, ny);
	}
	else
	{
		LuaSetProperty(l, property, propertyAddress, 3);
	}
}

int elements_loadDefault(lua_State * l)
{
	auto loadDefaultOne = [l](int id) {
		lua_getglobal(l, "elements");
		lua_pushnil(l);
		lua_setfield(l, -2, luaSim->elements[id].Identifier.c_str());

		ManageElementIdentifier(l, id, false);
		if (luaSim->elements[id].Init)
			luaSim->elements[id].Init(luaSim, &luaSim->elements[id], id);
		else
			luaSim->elements[id] = Element();
		ManageElementIdentifier(l, id, true);

		lua_pushinteger(l, id);
		lua_setfield(l, -2, luaSim->elements[id].Identifier.c_str());
		lua_pop(l, 1);
	};

	int args = lua_gettop(l);
	if (args)
	{
		luaL_checktype(l, 1, LUA_TNUMBER);
		int id = lua_tointeger(l, 1);
		if (id < 0 || id >= PT_NUM)
			return luaL_error(l, "Invalid element");
		loadDefaultOne(id);
	}
	else
	{
		for (int i = 0; i < PT_NUM; i++)
			loadDefaultOne(i);
	}

	FillMenus();
	for (auto moving = 0; moving < PT_NUM; ++moving)
	{
		for (auto into = 0; into < PT_NUM; ++into)
		{
			custom_can_move[moving][into] = 0;
		}
	}
	custom_init_can_move();
	memset(graphicscache, 0, sizeof(gcache_item)*PT_NUM);
	return 0;
}

int elements_allocate(lua_State * l)
{
	luaL_checktype(l, 1, LUA_TSTRING);
	luaL_checktype(l, 2, LUA_TSTRING);
	std::string group = std::string(tpt_lua_toString(l, 1));
	std::string id = std::string(tpt_lua_toString(l, 2));
	
	std::transform(group.begin(), group.end(), group.begin(), ::toupper);
	std::transform(id.begin(), id.end(), id.begin(), ::toupper);

	if (id.find('_') != id.npos)
		return luaL_error(l, "The element name may not contain '_'.");
	if (group.find('_') != id.npos)
		return luaL_error(l, "The group name may not contain '_'.");
	if (group == "DEFAULT")
		return luaL_error(l, "You cannot create elements in the 'DEFAULT' group.");

	std::stringstream identifierStream;
	identifierStream << group << "_PT_" << id;
	std::string identifier = identifierStream.str();

	for (int i = 0; i < PT_NUM; i++)
	{
		if (luaSim->elements[i].Enabled &&luaSim->elements[i].Identifier == identifier)
			return luaL_error(l, "Element identifier already in use");
	}

	int newID = -1;
	// Start out at 255 so that lua element IDs are still one byte (better save compatibility)
	for (int i = PT_NUM >= 255 ? 255 : PT_NUM; i >= 0; i--)
	{
		if (!luaSim->elements[i].Enabled)
		{
			newID = i;
			break;
		}
	}
	// If not enough space, then we start with the new maimum ID
	if (newID == -1)
	{
		for (int i = PT_NUM-1; i >= 255; i--)
		{
			if (!luaSim->elements[i].Enabled)
			{
				newID = i;
				break;
			}
		}
	}

	if (newID != -1)
	{
		luaSim->elements[newID] = Element();
		luaSim->elements[newID].Enabled = true;
		luaSim->elements[newID].Identifier = identifier;
		luaSim->elements[newID].MenuSection = SC_OTHER;
		menuSections[SC_OTHER]->AddTool(new Tool(INVALID_TOOL, identifier, "", "", 0, SC_OTHER, 0));

		lua_getglobal(l, "elements");
		lua_pushinteger(l, newID);
		lua_setfield(l, -2, identifier.c_str());
		lua_pop(l, 1);

		for (auto elem = 0; elem < PT_NUM; ++elem)
		{
			custom_can_move[elem][newID] = 0;
			custom_can_move[newID][elem] = 0;
		}
		custom_init_can_move();
		SetToolIndex(l, identifier, lastToolIndex++);
	}

	lua_pushinteger(l, newID);
	return 1;
}

int elements_element(lua_State * l)
{
	int id = luaL_checkinteger(l, 1);
	if (!luaSim->IsElementOrNone(id))
		return luaL_error(l, "Invalid element");

	if (lua_gettop(l) > 1)
	{
		luaL_checktype(l, 2, LUA_TTABLE);
		// Write values from native data to a table
		for (auto &prop : Element::GetProperties())
		{
			lua_getfield(l, -1, prop.Name.c_str());
			if (lua_type(l, -1) != LUA_TNIL)
			{
				auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id])) + prop.Offset);
				LuaSetProperty(l, prop, propertyAddress, -1);
			}
			lua_pop(l, 1);
		}

		lua_getfield(l, -1, "Update");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			lua_el_func[id].Assign(l, -1);
			lua_el_mode[id] = UPDATE_AFTER;
			luaSim->elements[id].Update = luaUpdateWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			lua_el_func[id].Clear();
			lua_el_mode[id] = UPDATE_AFTER;
			luaSim->elements[id].Update = luaSim->origElements[id].Update;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "Graphics");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			lua_gr_func[id].Assign(l, -1);
			luaSim->elements[id].Graphics = luaGraphicsWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			lua_gr_func[id].Clear();
			luaSim->elements[id].Graphics = luaSim->origElements[id].Graphics;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "CtypeDraw");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			luaCtypeDrawHandlers[id].Assign(l, -1);
			luaSim->elements[id].CtypeDraw = luaCtypeDrawWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			luaCtypeDrawHandlers[id].Clear();
			luaSim->elements[id].CtypeDraw = luaSim->origElements[id].CtypeDraw;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "Create");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			luaCreateHandlers[id].Assign(l, -1);
			luaSim->elements[id].Func_Create = luaCreateWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			luaCreateHandlers[id].Clear();
			luaSim->elements[id].Func_Create = luaSim->origElements[id].Func_Create;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "CreateAllowed");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			luaCreateAllowedHandlers[id].Assign(l, -1);
			luaSim->elements[id].Func_Create_Allowed = luaCreateAllowedWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			luaCreateAllowedHandlers[id].Clear();
			luaSim->elements[id].Func_Create_Allowed = luaSim->origElements[id].Func_Create_Allowed;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "ChangeType");
		if (lua_type(l, -1) == LUA_TFUNCTION)
		{
			luaChangeTypeHandlers[id].Assign(l, -1);
			luaSim->elements[id].Func_ChangeType = luaChangeTypeWrapper;
		}
		else if (lua_type(l, -1) == LUA_TBOOLEAN && !lua_toboolean(l, -1))
		{
			luaChangeTypeHandlers[id].Clear();
			luaSim->elements[id].Func_ChangeType = luaSim->origElements[id].Func_ChangeType;
		}
		lua_pop(l, 1);

		lua_getfield(l, -1, "DefaultProperties");
		SetDefaultProperties(l, id, -1);
		lua_pop(l, 1);

		FillMenus();
		custom_init_can_move();
		graphicscache[id].isready = 0;

		return 0;
	}
	else
	{
		// Write values from native data to a table
		lua_newtable(l);
		for (auto &prop : Element::GetProperties())
		{
			auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id])) + prop.Offset);
			LuaGetProperty(l, prop, propertyAddress);
			lua_setfield(l, -2, prop.Name.c_str());
		}

		tpt_lua_pushString(l, luaSim->elements[id].Identifier);
		lua_setfield(l, -2, "Identifier");

		GetDefaultProperties(l, id);
		lua_setfield(l, -2, "DefaultProperties");

		return 1;
	}
}

int elements_property(lua_State * l)
{
	int id = luaL_checkinteger(l, 1);
	if (!luaSim->IsElementOrNone(id))
		return luaL_error(l, "Invalid element");

	std::string propertyName = tpt_lua_checkString(l, 2);

	auto &properties = Element::GetProperties();
	auto prop = std::find_if(properties.begin(), properties.end(), [&propertyName](StructProperty const &p) {
		return p.Name == propertyName;
	});

	if (lua_gettop(l) > 2)
	{
		if (prop != properties.end())
		{
			if (lua_type(l, 3) != LUA_TNIL)
			{
				if (prop->Type == StructProperty::TransitionType)
				{
					int type = luaL_checkinteger(l, 3);
					if (!luaSim->IsElementOrNone(type) && type != NT && type != ST)
					{
						return luaL_error(l, "Invalid element");
					}
				}

				auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id])) + (*prop).Offset);
				ManageElementIdentifier(l, id, false);
				LuaSetProperty(l, *prop, propertyAddress, 3);
				ManageElementIdentifier(l, id, true);
			}

			FillMenus();
			custom_init_can_move();
			graphicscache[id].isready = 0;

			return 0;
		}
		else if (propertyName == "Update")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				switch (luaL_optint(l, 4, 0))
				{
				case 2:
					lua_el_mode[id] = UPDATE_BEFORE; //update before
					break;
				case 1:
					lua_el_mode[id] = UPDATE_REPLACE; //replace
					break;
				default:
					lua_el_mode[id] = UPDATE_AFTER; //update after
					break;
				}

				lua_el_func[id].Assign(l, 3);
				luaSim->elements[id].Update = luaUpdateWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				lua_el_func[id].Clear();
				lua_el_mode[id] = UPDATE_AFTER;
				luaSim->elements[id].Update = luaSim->origElements[id].Update;
			}
		}
		else if (propertyName == "Graphics")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				lua_gr_func[id].Assign(l, 3);
				graphicscache[id].isready = 0;
				luaSim->elements[id].Graphics = luaGraphicsWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				lua_gr_func[id].Clear();
				luaSim->elements[id].Graphics = luaSim->origElements[id].Graphics;;
			}
			graphicscache[id].isready = 0;
		}
		else if (propertyName == "CtypeDraw")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				luaCtypeDrawHandlers[id].Assign(l, 3);
				luaSim->elements[id].CtypeDraw = luaCtypeDrawWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				luaCtypeDrawHandlers[id].Clear();
				luaSim->elements[id].CtypeDraw = luaSim->origElements[id].CtypeDraw;
			}
			return 0;
		}
		else if (propertyName == "Create")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				luaCreateHandlers[id].Assign(l, 3);
				luaSim->elements[id].Func_Create = luaCreateWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				luaCreateHandlers[id].Clear();
				luaSim->elements[id].Func_Create = luaSim->origElements[id].Func_Create;
			}
			return 0;
		}
		else if (propertyName == "CreateAllowed")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				luaCreateAllowedHandlers[id].Assign(l, 3);
				luaSim->elements[id].Func_Create_Allowed = luaCreateAllowedWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				luaCreateAllowedHandlers[id].Clear();
				luaSim->elements[id].Func_Create_Allowed = luaSim->origElements[id].Func_Create_Allowed;
			}
		}
		else if (propertyName == "ChangeType")
		{
			if (lua_type(l, 3) == LUA_TFUNCTION)
			{
				luaChangeTypeHandlers[id].Assign(l, 3);
				luaSim->elements[id].Func_ChangeType = luaChangeTypeWrapper;
			}
			else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
			{
				luaChangeTypeHandlers[id].Clear();
				luaSim->elements[id].Func_ChangeType = luaSim->origElements[id].Func_ChangeType;
			}
		}
		else if (propertyName == "DefaultProperties")
		{
			SetDefaultProperties(l, id, 3);
		}
		else
		{
			return luaL_error(l, "Invalid element property");
		}
	}
	else
	{
		if (prop != properties.end())
		{
			auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id])) + (*prop).Offset);
			LuaGetProperty(l, *prop, propertyAddress);
			return 1;
		}
		else if (propertyName == "Identifier")
		{
			tpt_lua_pushString(l, luaSim->elements[id].Identifier);
			return 1;
		}
		else if (propertyName == "DefaultProperties")
		{
			GetDefaultProperties(l, id);
			return 1;
		}
		else
			return luaL_error(l, "Invalid element property");
	}
	return 0;
}

int elements_free(lua_State * l)
{
	int id = luaL_checkinteger(l, 1);
	if (!luaSim->IsElementOrNone(id))
		return luaL_error(l, "Invalid element");

	if (luaSim->elements[id].Identifier.find("DEFAULT_PT_") != luaSim->elements[id].Identifier.npos)
		return luaL_error(l, "Cannot free default elements");

	luaSim->elements[id].Enabled = 0;
	FillMenus();

	lua_getglobal(l, "elements");
	lua_pushnil(l);
	lua_setfield(l, -2, luaSim->elements[id].Identifier.c_str());
	lua_pop(l, 1);
	SetToolIndex(l, luaSim->elements[id].Identifier, -1);

	return 0;
}

int elements_exists(lua_State * l)
{
	lua_pushboolean(l, luaSim->IsElement(luaL_checkinteger(l, 1)));
	return 1;
}

int elements_getByName(lua_State * l)
{
	int t;
	std::string name = tpt_lua_checkString(l, 1);
	if (!console_parse_type(name.c_str(), &t, NULL, luaSim))
		t = -1;

	lua_pushinteger(l, t);
	return 1;
}

void GetDefaultProperties(lua_State * l, int id)
{
	lua_newtable(l);
	for (auto &prop : particle::GetProperties())
	{
		auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id].DefaultProperties)) + prop.Offset);
		LuaGetProperty(l, prop, propertyAddress);
		lua_setfield(l, -2, prop.Name.c_str());
	}
	for (auto &alias : particle::GetPropertyAliases())
	{
		lua_getfield(l, -1, alias.to.c_str());
		lua_setfield(l, -2, alias.from.c_str());
	}
}

void SetDefaultProperties(lua_State * l, int id, int stackPos)
{
	if (lua_type(l, stackPos) == LUA_TTABLE)
	{
		for (auto &prop : particle::GetProperties())
		{
			lua_getfield(l, stackPos, prop.Name.c_str());
			if (lua_type(l, -1) == LUA_TNIL)
			{
				for (auto &alias : particle::GetPropertyAliases())
				{
					if (alias.to == prop.Name)
					{
						lua_pop(l, 1);
						lua_getfield(l, stackPos, alias.from.c_str());
					}
				}
			}
			if (lua_type(l, -1) != LUA_TNIL)
			{
				auto propertyAddress = reinterpret_cast<intptr_t>((reinterpret_cast<unsigned char*>(&luaSim->elements[id].DefaultProperties)) + prop.Offset);
				LuaSetProperty(l, prop, propertyAddress, -1);
			}
			lua_pop(l, 1);
		}
	}
}

void ManageElementIdentifier(lua_State *l, int id, bool add)
{
	auto &elements = luaSim->elements;
	if (elements[id].Enabled)
	{
		lua_getglobal(l, "elements");
		tpt_lua_pushString(l, elements[id].Identifier);
		if (add)
		{
			lua_pushinteger(l, id);
		}
		else
		{
			lua_pushnil(l);
		}
		lua_settable(l, -3);
		lua_pop(l, 1);
	}
}

/*

TOOLS API

*/

int lastToolIndex = 0;
std::map<std::string, int> knownToolIndexes;

void initToolsAPI(lua_State * l)
{
	//Methods
	struct luaL_Reg toolsAPIMethods [] = {
		{"allocate", tools_allocate},
		{"property", tools_property},
		{"free", tools_free},
		{"exists", tools_exists},
		{"isCustom", tools_isCustom},
		{NULL, NULL}
	};
	luaL_register(l, "tools", toolsAPIMethods);

	lua_newtable(l);
	lua_setfield(l, -2, "index");
	for (int i = 0; i < SC_TOTAL; i++)
	{
		for (auto *tool : menuSections[i]->tools)
		{
			SetToolIndex(l, tool->GetIdentifier(), lastToolIndex++);
		}
	}
}

int tools_allocate(lua_State * l)
{
	luaL_checktype(l, 1, LUA_TSTRING);
	luaL_checktype(l, 2, LUA_TSTRING);
	auto group = Format::ToUpper(tpt_lua_toString(l, 1));
	auto name = Format::ToUpper(tpt_lua_toString(l, 2));
	if (name.find("_") != name.npos)
	{
		return luaL_error(l, "The tool name may not contain '_'.");
	}
	if (group.find("_") != name.npos)
	{
		return luaL_error(l, "The group name may not contain '_'.");
	}
	if (group == "DEFAULT")
	{
		return luaL_error(l, "You cannot create tools in the 'DEFAULT' group.");
	}
	std::string identifier = group + "_TOOL_" + name;
	if (knownToolIndexes.find(identifier) != knownToolIndexes.end())
	{
		return luaL_error(l, "Tool identifier already in use.");
	}
	int index = lastToolIndex++;
	{
		luaTools[index] = LuaToolData(index, name, COLRGB(255, 255, 255), identifier, "No description provided.", SC_TOOL, 1);
		luaToolRefs[index] = CustomTool();
	}
	FillMenus();
	lua_pushinteger(l, index);
	SetToolIndex(l, identifier, index);
	return 1;
}

template <typename T>
struct DependentFalse : std::false_type
{
};

bool IsCustom(int index)
{
	return luaTools.find(index)->second.index;
}

int tools_property(lua_State * l)
{
	int index = luaL_checkinteger(l, 1);
	bool isCustom = IsCustom(index);
	Tool *tool = GetToolByIndex(index);
	if (!tool)
	{
		return luaL_error(l, "Invalid tool");
	}
	LuaToolData *luaToolData = &luaTools[index];
	auto toolRefs = &luaToolRefs[index];

	std::string propertyName = tpt_lua_checkString(l, 2);
	auto handleCallback = [l, &toolRefs, &propertyName, &index](
		auto customToolMember,
		const char *luaPropertyName
	) {
		if (propertyName == luaPropertyName)
		{
			if (lua_gettop(l) > 2)
			{
				if (luaTools.find(index) == luaTools.end())
				{
					luaL_error(l, "Cannot change callbacks of default tools");
				}
				if (lua_type(l, 3) == LUA_TFUNCTION)
				{
					(toolRefs->*customToolMember).Assign(l, 3);
				}
				else if (lua_type(l, 3) == LUA_TBOOLEAN && !lua_toboolean(l, 3))
				{
					(toolRefs->*customToolMember).Clear();
				}
				return true;
			}
			luaL_error(l, "Invalid tool property");
		}
		return false;
	};
	if (handleCallback(&CustomTool::perform , "Perform" ) ||
		handleCallback(&CustomTool::click   , "Click"   ) ||
		handleCallback(&CustomTool::drag    , "Drag"    ) ||
		handleCallback(&CustomTool::draw    , "Draw"    ) ||
		handleCallback(&CustomTool::drawLine, "DrawLine") ||
		handleCallback(&CustomTool::drawRect, "DrawRect") ||
		handleCallback(&CustomTool::drawFill, "DrawFill") ||
		handleCallback(&CustomTool::select  , "Select"  ))
	{
		return 0;
	}

	if (propertyName == "Identifier")
	{
		tpt_lua_pushString(l, tool->GetIdentifier());
		return 1;
	}

	// Had to separate this out into custom tool ver. and non-custom tool ver.
	// Because Tool* objects are too temporary in my mod to set their values
	if (isCustom)
	{
		int returnValueCount = 0;
		auto handleProperty = [l, &luaToolData, &propertyName, &returnValueCount](
			auto toolDataMember,
			const char *luaPropertyName,
			bool buildMenusIfChanged
		) {
			if (propertyName == luaPropertyName)
			{
				auto &thing = luaToolData->*toolDataMember;
				using PropertyType = std::remove_reference_t<decltype(thing)>;
				if (lua_gettop(l) > 2)
				{
					if      constexpr (std::is_same_v<PropertyType, std::string >) thing = tpt_lua_checkString(l, 3);
					else if constexpr (std::is_same_v<PropertyType, bool        >) thing =lua_toboolean(l, 3);
					else if constexpr (std::is_same_v<PropertyType, int         >) thing =luaL_checkinteger(l, 3);
					else if constexpr (std::is_same_v<PropertyType, ARGBColour  >) thing =luaL_checkinteger(l, 3);
					else static_assert(DependentFalse<PropertyType>::value);
					if (buildMenusIfChanged)
					{
						FillMenus();
					}
				}
				else
				{
					if      constexpr (std::is_same_v<PropertyType, std::string >) tpt_lua_pushString(l, thing);
					else if constexpr (std::is_same_v<PropertyType, bool        >) lua_pushboolean(l, thing);
					else if constexpr (std::is_same_v<PropertyType, int         >) lua_pushinteger(l, thing);
					else if constexpr (std::is_same_v<PropertyType, ARGBColour  >) lua_pushinteger(l, thing);
					else static_assert(DependentFalse<PropertyType>::value);
					returnValueCount = 1;
				}
				return true;
			}
			return false;
		};
		if (handleProperty(&LuaToolData::name        , "Name",  true) ||
			handleProperty(&LuaToolData::description , "Description",  true) ||
			handleProperty(&LuaToolData::color       , "Colour"     ,  true) ||
			handleProperty(&LuaToolData::color       , "Color"      ,  true) ||
			handleProperty(&LuaToolData::menuSection , "MenuSection",  true) ||
			handleProperty(&LuaToolData::menuVisible , "MenuVisible",  true))
		{
			return returnValueCount;
		}
	}
	else
	{
		if (lua_gettop(l) > 2)
		{
			return luaL_error(l, "Can only change properties of custom tools");
		}
		auto handleProperty = [l, &tool, &propertyName](auto toolGetter, const char *luaPropertyName, bool buildMenusIfChanged) {
			if (propertyName == luaPropertyName)
			{
				auto thing = (tool->*toolGetter)();
				using PropertyType = std::remove_reference_t<decltype(thing)>;

					if      constexpr (std::is_same_v<PropertyType, std::string >) tpt_lua_pushString(l, thing);
					else if constexpr (std::is_same_v<PropertyType, bool        >) lua_pushboolean(l, thing);
					else if constexpr (std::is_same_v<PropertyType, int         >) lua_pushinteger(l, thing);
					else if constexpr (std::is_same_v<PropertyType, ARGBColour  >) lua_pushinteger(l, thing);
					else static_assert(DependentFalse<PropertyType>::value);
				return true;
			}
			return false;
		};
		if (handleProperty(&Tool::GetName        , "Name"       ,  true) ||
			handleProperty(&Tool::GetDescription , "Description",  true) ||
			handleProperty(&Tool::GetColor       , "Colour"     ,  true) ||
			handleProperty(&Tool::GetColor       , "Color"      ,  true) ||
			handleProperty(&Tool::GetMenuSection , "MenuSection",  true) ||
			handleProperty(&Tool::GetMenuVisible , "MenuVisible",  true))
		{
			return 1;
		}
	}

	return luaL_error(l, "Invalid tool property");
}

int tools_free(lua_State * l)
{
	int index = luaL_checkinteger(l, 1);
	auto *tool = GetToolByIndex(index);
	if (!tool)
	{
		return luaL_error(l, "Invalid tool");
	}
	if (!IsCustom(index))
	{
		return luaL_error(l, "Can only free custom tools");
	}
	luaTools.erase(index);
	luaToolRefs.erase(index);
	FillMenus();
	return 0;
}

int tools_exists(lua_State * l)
{
	int index = luaL_checkinteger(l, 1);
	lua_pushboolean(l, bool(GetToolByIndex(index)));
	return 1;
}

int tools_isCustom(lua_State * l)
{
	int index = luaL_checkinteger(l, 1);
	Tool *tool = GetToolByIndex(index);
	if (!tool)
	{
		return luaL_error(l, "Invalid tool");
	}
	lua_pushboolean(l, IsCustom(index));
	return 1;
}

Tool * GetToolByIndex(int index)
{
	for (auto entry : knownToolIndexes)
	{
		if (entry.second == index)
			return GetToolFromIdentifier(entry.first);
	}

	return nullptr;
}

void SetToolIndex(lua_State *l, std::string identifier, int index)
{
	lua_getglobal(l, "tools");
	lua_getfield(l, -1, "index");
	tpt_lua_pushString(l, identifier);
	if (index != -1)
	{
		lua_pushinteger(l, index);
	}
	else
	{
		lua_pushnil(l);
	}
	lua_settable(l, -3);
	lua_pop(l, 2);

	knownToolIndexes[identifier] = index;
}

void initPlatformAPI(lua_State * l)
{
	struct luaL_Reg platformAPIMethods [] = {
		{"platform", platform_platform},
		{"build", platform_build},
		{"releaseType", platform_releaseType},
		{"exeName", platform_exeName},
		{"restart", platform_restart},
		{"openLink", platform_openLink},
		{"clipboardCopy", platform_clipboardCopy},
		{"clipboardPaste", platform_clipboardPaste},
		{"showOnScreenKeyboard", platform_showOnScreenKeyboard},
		{"getOnScreenKeyboardInput", platform_getOnScreenKeyboardInput},
		{NULL, NULL}
	};
	luaL_register(l, "platform", platformAPIMethods);

	lua_getglobal(l, "platform");
	lua_setglobal(l, "plat");
}

int platform_platform(lua_State * l)
{
	tpt_lua_pushString(l, IDENT_PLATFORM);
	return 1;
}

int platform_build(lua_State * l)
{
	tpt_lua_pushString(l, IDENT_BUILD);
	return 1;
}

int platform_releaseType(lua_State * l)
{
	tpt_lua_pushString(l, IDENT_RELTYPE);
	return 1;
}

int platform_exeName(lua_State * l)
{
	char *name = Platform::ExecutableName();
	if (name)
		tpt_lua_pushString(l, name);
	else
		luaL_error(l, "Error, could not get executable name");
	free(name);
	return 1;
}

int platform_restart(lua_State * l)
{
	int saveTab = luaL_optinteger(l, 1, 0);
	Platform::DoRestart(saveTab ? true : false, false);
	return 0;
}

int platform_openLink(lua_State * l)
{
	std::string uri = tpt_lua_checkString(l, 1);
	Platform::OpenLink(uri);
	return 0;
}

int platform_clipboardCopy(lua_State * l)
{
	std::string text = Engine::Ref().ClipboardPull();
	tpt_lua_pushString(l, text);
	return 1;
}

int platform_clipboardPaste(lua_State * l)
{
	luaL_checktype(l, 1, LUA_TSTRING);
	Engine::Ref().ClipboardPush(tpt_lua_optString(l, 1, ""));
	return 0;
}

int platform_showOnScreenKeyboard(lua_State * l)
{
	std::string startText = tpt_lua_optString(l, 1, "");
	int acount = lua_gettop(l);
	bool autoCorrect = false;
	if (acount > 1)
	{
		luaL_checktype(l, 2, LUA_TBOOLEAN);
		autoCorrect = lua_toboolean(l, 2);
	}
	Platform::ShowOnScreenKeyboard(startText.c_str(), autoCorrect);
	return 0;
}

int platform_getOnScreenKeyboardInput(lua_State * l)
{
	int acount = lua_gettop(l);
	if (acount)
		luaL_checktype(l, 1, LUA_TSTRING);
	int limit = luaL_optint(l, 2, 1024);
	if (limit < 0 || limit > 2048)
		luaL_error(l, "Error, string size too long");
	std::string startText = tpt_lua_optString(l, 1, "");
	char *buff = (char*)calloc(limit+1, sizeof(char));
	strncpy(buff, startText.c_str(), limit);
	bool autoCorrect = false;
	if (acount > 2)
	{
		luaL_checktype(l, 3, LUA_TBOOLEAN);
		autoCorrect = lua_toboolean(l, 3);
	}
	Platform::GetOnScreenKeyboardInput(buff, limit, autoCorrect);
	tpt_lua_pushString(l, buff);
	free(buff);
	return 1;
}

void initEventAPI(lua_State * l)
{
	struct luaL_Reg eventAPIMethods [] = {
		{"register", event_register},
		{"unregister", event_unregister},
		{"getModifiers", event_getmodifiers},
		{NULL, NULL}
	};
	luaL_register(l, "event", eventAPIMethods);

	lua_getglobal(l, "event");
	lua_setglobal(l, "evt");

	lua_pushinteger(l, LuaEvents::keypress); lua_setfield(l, -2, "KEYPRESS");
	lua_pushinteger(l, LuaEvents::keyrelease); lua_setfield(l, -2, "KEYRELEASE");
	lua_pushinteger(l, LuaEvents::textinput); lua_setfield(l, -2, "TEXTINPUT");
	lua_pushinteger(l, LuaEvents::mousedown); lua_setfield(l, -2, "MOUSEDOWN");
	lua_pushinteger(l, LuaEvents::mouseup); lua_setfield(l, -2, "MOUSEUP");
	lua_pushinteger(l, LuaEvents::mousemove); lua_setfield(l, -2, "MOUSEMOVE");
	lua_pushinteger(l, LuaEvents::mousewheel); lua_setfield(l, -2, "MOUSEWHEEL");
	lua_pushinteger(l, LuaEvents::tick); lua_setfield(l, -2, "TICK");
	lua_pushinteger(l, LuaEvents::blur); lua_setfield(l, -2, "BLUR");
	lua_pushinteger(l, LuaEvents::close); lua_setfield(l, -2, "CLOSE");
	lua_pushinteger(l, LuaEvents::beforesim); lua_setfield(l, -2, "BEFORESIM");
	lua_pushinteger(l, LuaEvents::aftersim); lua_setfield(l, -2, "AFTERSIM");
	lua_pushinteger(l, LuaEvents::beforesimdraw); lua_setfield(l, -2, "BEFORESIMDRAW");
	lua_pushinteger(l, LuaEvents::aftersimdraw); lua_setfield(l, -2, "AFTERSIMDRAW");
}

int event_register(lua_State * l)
{
	int eventName = luaL_checkinteger(l, 1);
	luaL_checktype(l, 2, LUA_TFUNCTION);
	return LuaEvents::RegisterEventHook(l, "tptevents-" + Format::NumberToString<int>(eventName));
}

int event_unregister(lua_State * l)
{
	int eventName = luaL_checkinteger(l, 1);
	luaL_checktype(l, 2, LUA_TFUNCTION);
	return LuaEvents::UnregisterEventHook(l, "tptevents-" + Format::NumberToString<int>(eventName));
}

int event_getmodifiers(lua_State * l)
{
	lua_pushnumber(l, Engine::Ref().GetModifiers());
	return 1;
}

class RequestHandle
{
public:
	enum RequestType
	{
		normal,
		getAuthToken,
	};

private:
	Request *request;
	bool dead;
	RequestType type;

	RequestHandle() = default;

	void FinishGetAuthToken(std::string &data, int &status_out, std::vector<http::Header> &headers)
	{
		headers.clear();
		std::istringstream ss(data);
		Json::Value root;
		try
		{
			ss >> root;
			auto status = root["Status"].asString();
			if (status == "OK")
			{
				status_out = 200;
				data = root["Token"].asString();
			}
			else
			{
				status_out = 403;
				data = status;
			}
		}
		catch (std::exception &e)
		{
			std::cerr << "bad auth response: " << e.what() << std::endl;
			status_out = 600;
			data.clear();
		}
	}

public:
	static int Make(lua_State *l, const std::string &uri, bool isPost, const std::string &verb, RequestType type, const http::PostData &postData, const std::vector<http::Header> &headers)
	{
		if (type == getAuthToken && !svf_login)
		{
			lua_pushnil(l);
			lua_pushliteral(l, "not authenticated");
			return 2;
		}
		auto *rh = (RequestHandle *)lua_newuserdata(l, sizeof(RequestHandle));
		if (!rh)
		{
			return 0;
		}
		new(rh) RequestHandle();
		rh->type = type;
		rh->request = new Request(uri);
		if (verb.size())
		{
			rh->request->Verb(verb);
		}
		for (const auto &header : headers)
		{
			rh->request->AddHeader(header);
		}
		if (isPost)
		{
			rh->request->AddPostData(postData);
		}
		if (type == getAuthToken)
		{
			rh->request->AuthHeaders(svf_user_id, svf_session_id);
		}
		rh->request->Start();
		luaL_newmetatable(l, "HTTPRequest");
		lua_setmetatable(l, -2);
		return 1;
	}

	RequestHandle(std::string &uri, http::PostData &post_data, std::vector<http::Header> &headers)
	{
		dead = false;
		request = new Request(uri);
		for (auto &header : headers)
		{
			request->AddHeader(header);
		}
		request->AddPostData(post_data);
		request->Start();
	}

	~RequestHandle()
	{
		if (!Dead())
		{
			Cancel();
		}
	}

	bool Dead() const
	{
		return dead;
	}

	bool Done() const
	{
		return dead || request->CheckDone();
	}

	void Progress(int *total, int *done)
	{
		if (!dead)
		{
			request->CheckProgress(total, done);
		}
	}

	void Cancel()
	{
		if (!dead)
		{
			request->Cancel();
			dead = true;
		}
	}

	std::string Finish(int &status_out, std::vector<http::Header> &headers)
	{
		std::string data;
		if (!dead)
		{
			if (request->CheckDone())
			{
				data = request->Finish(&status_out, &headers);
				if (type == getAuthToken && status_out == 200)
				{
					FinishGetAuthToken(data, status_out, headers);
				}
				dead = true;
			}
		}
		return data;
	}
};

int http_request_gc(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	rh->~RequestHandle();
	return 0;
}

int http_request_status(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	if (rh->Dead())
	{
		lua_pushliteral(l, "dead");
	}
	else if (rh->Done())
	{
		lua_pushliteral(l, "done");
	}
	else
	{
		lua_pushliteral(l, "running");
	}
	return 1;
}

int http_request_progress(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	if (!rh->Dead())
	{
		int total, done;
		rh->Progress(&total, &done);
		lua_pushinteger(l, total);
		lua_pushinteger(l, done);
		return 2;
	}
	return 0;
}

int http_request_cancel(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	if (!rh->Dead())
	{
		rh->Cancel();
	}
	return 0;
}

int http_request_finish(lua_State *l)
{
	auto *rh = (RequestHandle *)luaL_checkudata(l, 1, "HTTPRequest");
	if (!rh->Dead())
	{
		int status_out;
		std::vector<http::Header> headers;
		std::string data = rh->Finish(status_out, headers);
		lua_pushlstring(l, data.c_str(), data.size());
		lua_pushinteger(l, status_out);
		lua_newtable(l);
		for (auto i = 0; i < int(headers.size()); ++i)
		{
			lua_newtable(l);
			lua_pushlstring(l, headers[i].name.data(), headers[i].name.size());
			lua_rawseti(l, -2, 1);
			lua_pushlstring(l, headers[i].value.data(), headers[i].value.size());
			lua_rawseti(l, -2, 2);
			lua_rawseti(l, -2, i + 1);
		}
		return 3;
	}
	return 0;
}

int http_request(lua_State *l, bool isPost)
{
	std::string uri = tpt_lua_checkString(l, 1);

	http::PostData postData;
	auto headersIndex = 2;
	auto verbIndex = 3;

	if (isPost)
	{
		headersIndex += 1;
		verbIndex += 1;

		if (lua_isstring(l, 2))
		{
			postData = tpt_lua_toString(l, 2);
		}
		else if (lua_istable(l, 2))
		{
			postData = http::FormData{};
			auto &formData = std::get<http::FormData>(postData);
			int size = lua_objlen(l, 2);
			if (size)
			{
				for (int i = 0; i < size; ++i)
				{
					lua_rawgeti(l, 2, i + 1);
					if (!lua_istable(l, -1))
					{
						luaL_error(l, "form item %i is not a table", i + 1);
					}
					lua_rawgeti(l, -1, 1);
					if (!lua_isstring(l, -1))
					{
						luaL_error(l, "name of form item %i is not a string", i + 1);
					}
					auto name = tpt_lua_toString(l, -1);
					lua_pop(l, 1);
					lua_rawgeti(l, -1, 2);
					if (!lua_isstring(l, -1))
					{
						luaL_error(l, "value of form item %i is not a string", i + 1);
					}
					auto value = tpt_lua_toString(l, -1);
					lua_pop(l, 1);
					std::optional<std::string> filename;
					lua_rawgeti(l, -1, 3);
					if (!lua_isnoneornil(l, -1))
					{
						if (!lua_isstring(l, -1))
						{
							luaL_error(l, "filename of form item %i is not a string", i + 1);
						}
						filename = tpt_lua_toString(l, -1);
					}
					lua_pop(l, 1);
					formData.push_back({ name, value, filename });
					lua_pop(l, 1);
				}
			}
			else
			{
				lua_pushnil(l);
				while (lua_next(l, 2))
				{
					lua_pushvalue(l, -2);
					formData.push_back({ tpt_lua_toString(l, -1), tpt_lua_toString(l, -2) });
					lua_pop(l, 2);
				}
			}
		}
	}

	std::vector<http::Header> headers;
	if (lua_istable(l, headersIndex))
	{
		auto size = lua_objlen(l, headersIndex);
		if (size)
		{
			for (auto i = 0U; i < size; ++i)
			{
				lua_rawgeti(l, headersIndex, i + 1);
				if (!lua_istable(l, -1))
				{
					luaL_error(l, "header %i is not a table", i + 1);
				}
				lua_rawgeti(l, -1, 1);
				if (!lua_isstring(l, -1))
				{
					luaL_error(l, "name of header %i is not a string", i + 1);
				}
				auto name = tpt_lua_toString(l, -1);
				lua_pop(l, 1);
				lua_rawgeti(l, -1, 2);
				if (!lua_isstring(l, -1))
				{
					luaL_error(l, "value of header %i is not a string", i + 1);
				}
				auto value = tpt_lua_toString(l, -1);
				lua_pop(l, 1);
				headers.push_back({ name, value });
				lua_pop(l, 1);
			}
		}
		else
		{
			// old dictionary format
			lua_pushnil(l);
			while (lua_next(l, headersIndex))
			{
				lua_pushvalue(l, -2);
				headers.push_back({ tpt_lua_toString(l, -1), tpt_lua_toString(l, -2) });
				lua_pop(l, 2);
			}
		}
	}

	auto verb = tpt_lua_optString(l, verbIndex, "");
	return RequestHandle::Make(l, uri, isPost, verb, RequestHandle::normal, postData, headers);
}

int http_get(lua_State *l)
{
	return http_request(l, false);
}

int http_post(lua_State *l)
{
	return http_request(l, true);
}

int http_get_auth_token(lua_State *l)
{
	return RequestHandle::Make(l, SCHEME SERVER "/ExternalAuth.api?Action=Get&Audience=" + Format::URLEncode(tpt_lua_checkString(l, 1)), false, {}, RequestHandle::getAuthToken, {}, {});
}

void initHttpAPI(lua_State *l)
{
	luaL_newmetatable(l, "HTTPRequest");
	lua_pushcfunction(l, http_request_gc);
	lua_setfield(l, -2, "__gc");
	lua_newtable(l);
	struct luaL_Reg httpRequestIndexMethods[] = {
		{ "status", http_request_status },
		{ "progress", http_request_progress },
		{ "cancel", http_request_cancel },
		{ "finish", http_request_finish },
		{ NULL, NULL }
	};
	luaL_register(l, NULL, httpRequestIndexMethods);
	lua_setfield(l, -2, "__index");
	lua_pop(l, 1);
	lua_newtable(l);
	struct luaL_Reg httpAPIMethods [] = {
		{"get", http_get},
		{"post", http_post},
		{"getAuthToken", http_get_auth_token},
		{NULL, NULL}
	};
	luaL_register(l, "http", httpAPIMethods);
}

void initSocketAPI(lua_State * l)
{
	LuaTCPSocket::Open(l);
}

void initBZ2API(lua_State *l)
{
	luaL_Reg reg[] = {
		{ "compress", bz2_compress_wrapper },
		{ "decompress", bz2_decompress_wrapper },
		{ NULL, NULL },
	};
	lua_newtable(l);
	luaL_register(l, NULL, reg);
#define BZ2_CONST(k, v) lua_pushinteger(l, int(v)); lua_setfield(l, -2, k)
	BZ2_CONST("COMPRESS_NOMEM", BZ2WCompressNomem);
	BZ2_CONST("COMPRESS_LIMIT", BZ2WCompressLimit);
	BZ2_CONST("DECOMPRESS_NOMEM", BZ2WDecompressNomem);
	BZ2_CONST("DECOMPRESS_LIMIT", BZ2WDecompressLimit);
	BZ2_CONST("DECOMPRESS_TYPE", BZ2WDecompressType);
	BZ2_CONST("DECOMPRESS_BAD", BZ2WDecompressBad);
	BZ2_CONST("DECOMPRESS_EOF", BZ2WDecompressEof);
#undef BZ2_CONST
	lua_setglobal(l, "bz2");
}

int bz2_compress_wrapper(lua_State *l)
{
	auto src = tpt_lua_checkString(l, 1);
	auto maxSize = size_t(luaL_optinteger(l, 2, 0));
	std::vector<char> dest;
	auto result = BZ2WCompress(dest, src.data(), src.size(), maxSize);
#define RETURN_ERR(str) lua_pushnil(l); lua_pushinteger(l, int(result)); lua_pushliteral(l, str); return 3
	switch (result)
	{
	case BZ2WCompressOk: break;
	case BZ2WCompressNomem: RETURN_ERR("out of memory");
	case BZ2WCompressLimit: RETURN_ERR("size limit exceeded");
	}
#undef RETURN_ERR
	tpt_lua_pushString(l, std::string(dest.begin(), dest.end()));
	return 1;
}

int bz2_decompress_wrapper(lua_State *l)
{
	auto src = tpt_lua_checkString(l, 1);
	auto maxSize = size_t(luaL_optinteger(l, 2, 0));
	std::vector<char> dest;
	auto result = BZ2WDecompress(dest, src.data(), src.size(), maxSize);
#define RETURN_ERR(str) lua_pushnil(l); lua_pushinteger(l, int(result)); lua_pushliteral(l, str); return 3
	switch (result)
	{
	case BZ2WDecompressOk: break;
	case BZ2WDecompressNomem: RETURN_ERR("out of memory");
	case BZ2WDecompressLimit: RETURN_ERR("size limit exceeded");
	case BZ2WDecompressType:
	case BZ2WDecompressBad:
	case BZ2WDecompressEof: RETURN_ERR("corrupted stream");
	}
#undef RETURN_ERR
	tpt_lua_pushString(l, std::string(dest.begin(), dest.end()));
	return 1;
}

void tpt_lua_pushString(lua_State *L, const std::string &str)
{
	lua_pushlstring(L, str.data(), str.size());
}

std::string tpt_lua_toString(lua_State *L, int index)
{
	size_t size;
	if (auto *data = lua_tolstring(L, index, &size))
	{
		return std::string(data, size);
	}
	return {};
}

std::string tpt_lua_checkString(lua_State *L, int index)
{
	size_t size;
	if (auto *data = luaL_checklstring(L, index, &size))
	{
		return std::string(data, size);
	}
	return {};
}

std::string tpt_lua_optString(lua_State *L, int index, std::string defaultValue)
{
	if (lua_isnoneornil(L, index))
	{
		return defaultValue;
	}
	return tpt_lua_checkString(L, index);
}

int tpt_lua_loadstring(lua_State *L, const std::string &str)
{
	return luaL_loadbuffer(L, str.data(), str.size(), str.data());
}

int tpt_lua_dostring(lua_State *L, const std::string &str)
{
	return tpt_lua_loadstring(L, str) || tpt_lua_pcall(L, 0, LUA_MULTRET, 0);
}

bool tpt_lua_equalsString(lua_State *L, int index, const char *data, size_t size)
{
	return lua_isstring(L, index) && lua_objlen(L, index) == size && !memcmp(lua_tostring(L, index), data, size);
}

long unsigned int luaExecutionStart = 0;
int luaHookTimeout = 3000;
int tpt_lua_pcall(lua_State *L, int numArgs, int numResults, int errorFunc, EventTraits eventTrait)
{
	::eventTrait = eventTrait;
	luaExecutionStart = Platform::GetTime();
	return lua_pcall(L, numArgs, numResults, errorFunc);
	::eventTrait = eventTraitNone;
}

int tpt_lua_pcall(lua_State *L, int numArgs, int numResults, int errorFunc)
{
	return tpt_lua_pcall(L, numArgs, numResults, errorFunc, eventTraitNone);
}

#endif
