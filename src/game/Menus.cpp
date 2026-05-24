
#include "Menus.h"
#include "Favorite.h"
#include "hud.h"
#include "common/Translations.h"
#include "lua/LuaTool.h"
#include "simulation/Simulation.h"
#include "simulation/Tool.h"
#include "simulation/WallNumbers.h"
#include "simulation/ToolNumbers.h"
#include "simulation/GolNumbers.h"
#include "simulation/elements/LIFE.h"

void MenuSection::ClearTools()
{
	for (std::vector<Tool*>::iterator iter = tools.begin(), end = tools.end(); iter != end; ++iter)
		delete *iter;
	tools.clear();
}

void MenuSection::AddTool(Tool* tool)
{
	auto it = std::upper_bound(tools.begin(), tools.end(), tool, [](auto a, auto b){ return a->GetMenuSort() < b->GetMenuSort(); });
	tools.insert(it, tool);
}

MenuSection* menuSections[SC_TOTAL];
bool stickyCategories = false;

void InitMenusections()
{
	menuSections[SC_WALL]      = new MenuSection('\xC1', "Walls", true, false);
	menuSections[SC_ELEC]      = new MenuSection('\xC2', "Electronics", true, false);
	menuSections[SC_POWERED]   = new MenuSection('\xD6', "Powered Materials", true, false);
	menuSections[SC_SENSOR]    = new MenuSection('\x99', "Sensors", true, false);
	menuSections[SC_FORCE]     = new MenuSection('\xE3', "Force Creating", true, false);
	menuSections[SC_EXPLOSIVE] = new MenuSection('\xC3', "Explosives", true, false);
	menuSections[SC_GAS]       = new MenuSection('\xC5', "Gases", true, false);
	menuSections[SC_LIQUID]    = new MenuSection('\xC4', "Liquids", true, false);
	menuSections[SC_POWDERS]   = new MenuSection('\xD0', "Powders", true, false);
	menuSections[SC_SOLIDS]    = new MenuSection('\xD1', "Solids", true, false);
	menuSections[SC_NUCLEAR]   = new MenuSection('\xC6', "Radioactive", true, false);
	menuSections[SC_SPECIAL]   = new MenuSection('\xCC', "Special", true, false);
	menuSections[SC_LIFE]      = new MenuSection('\xD2', "Game of Life", true, false);
	menuSections[SC_TOOL]      = new MenuSection('\xD7', "Tools", true, false);
	menuSections[SC_FAV]       = new MenuSection('\xE2', "Favorites & Recents", true, false);
#ifdef NOMOD
	menuSections[SC_DECO]      = new MenuSection('\xE5', "Deco", true, true);
	menuSections[SC_OTHER]     = new MenuSection('\xE2', "Other", false, false); //list of elements that are hidden or disabled, not in any menu
#ifdef TOUCHUI
	menuSections[SC_SEARCH]    = new MenuSection('\xE6', "Search", true, true);
#endif
#else
	menuSections[SC_DECO]      = new MenuSection('\xE5', "Deco", true, true);
	menuSections[SC_FAV2]      = new MenuSection('\xE2', "Favorite2", false, false);
	menuSections[SC_HUD]       = new MenuSection('\xE2', "HUD", false, false);
	menuSections[SC_OTHER]     = new MenuSection('\xE2', "Other", false, false); //list of elements that are hidden or disabled, not in any menu
	menuSections[SC_SEARCH]    = new MenuSection('\xE6', "Search", false, true);
#endif
}

void ClearMenusections()
{
	delete GetToolFromIdentifier("DEFAULT_FAV_MORE");
	menuSections[SC_FAV]->tools.clear();
	for (int i = 0; i < SC_TOTAL; i++)
	{
		if (i != SC_FAV)
			menuSections[i]->ClearTools();
		delete menuSections[i];
	}
}

int GetNumMenus(bool onlyEnabled)
{
	int total = 0;
	for (int j = 0; j < SC_TOTAL; j++)
		if (!onlyEnabled || menuSections[j]->enabled)
			total++;
	return total;
}

int GetMenuSection(Tool *tool)
{
	for (int i = 0; i < SC_TOTAL; i++)
	{
		for (std::vector<Tool*>::iterator iter = menuSections[i]->tools.begin(), end = menuSections[i]->tools.end(); iter != end; ++iter)
		{
			if (tool == (*iter))
				return i;
		}
	}
	return -1;
}

//fills all the menus with Tool*s
void FillMenus()
{
	std::string tempActiveTools[3];
	if (activeTools[0])
	{
		for (int i = 0; i < 3; i++)
			tempActiveTools[i] = activeTools[i]->GetIdentifier();
	}
	PropTool* propTool = (PropTool*)GetToolFromIdentifier("DEFAULT_UI_PROPERTY");
	if (propTool)
		propTool = new PropTool(*propTool);
#ifndef NOMOD
	delete GetToolFromIdentifier("DEFAULT_FAV_MORE");
#endif
	//Clear all menusections
	for (int i = 0; i < SC_TOTAL; i++)
	{
		if (i != SC_FAV)
			menuSections[i]->ClearTools();
	}
	menuSections[SC_FAV]->tools.clear();

	//Add all generic elements to menus
	for (int i = 0; i < PT_NUM; i++)
	{
#ifndef NOMOD
		if (i == PT_EXPL && !explUnlocked)
		{
			menuSections[SC_OTHER]->AddTool(new ElementTool(globalSim, i));
			continue;
		}
#endif
		if (globalSim->elements[i].Enabled && i != PT_LIFE)
		{
			if ((globalSim->elements[i].MenuVisible || secret_els) && globalSim->elements[i].MenuSection >= 0 && globalSim->elements[i].MenuSection < SC_TOTAL && globalSim->elements[i].MenuSection != SC_FAV)
			{
				if (i == PT_STKM || i == PT_STKM2 || i == PT_FIGH)
					menuSections[globalSim->elements[i].MenuSection]->AddTool(new PlopTool(globalSim, i));
				else
					menuSections[globalSim->elements[i].MenuSection]->AddTool(new ElementTool(globalSim, i));
			}
			else
				menuSections[SC_OTHER]->AddTool(new ElementTool(globalSim, i));
		}
	}

	//Fill up LIFE menu
	for (int i = 0; i < NGOL; i++)
	{
		menuSections[SC_LIFE]->AddTool(new GolTool(i));
	}
	for (auto &cgol : static_cast<LIFE_ElementDataContainer&>(*globalSim->elementData[PT_LIFE]).GetCustomGOL())
	{
		menuSections[SC_LIFE]->AddTool(new GolTool(cgol.rule, cgol.nameString, "Custom GOL type: " + cgol.ruleString, cgol.color1));
	}

	//Fill up wall menu
	for (int i = 0; i < WALLCOUNT; i++)
	{
		if (i == WL_STREAM)
			menuSections[SC_WALL]->AddTool(new StreamlineTool());
		else
			menuSections[SC_WALL]->AddTool(new WallTool(i));
	}

	//Fill up tools menu
	for (int i = 0; i < TOOLCOUNT; i++)
	{
		if (i == TOOL_PROP)
		{
			if (propTool)
				menuSections[SC_TOOL]->AddTool(propTool);
			else
				menuSections[SC_TOOL]->AddTool(new PropTool());
		}
		else if (i == TOOL_GOL)
			menuSections[SC_LIFE]->AddTool(new ToolTool(i));
		else
			menuSections[SC_TOOL]->AddTool(new ToolTool(i));
	}

	//Fill up deco menu
	for (int i = 0; i < DECOCOUNT; i++)
	{
		menuSections[SC_DECO]->AddTool(new DecoTool(i));
	}

	for (int n = 0; n < NUM_COLOR_PRESETS; n++)
	{
		menuSections[SC_OTHER]->AddTool(new DecoPresetTool(n));
	}

	//Fill up fav. related menus somehow ...
#ifndef NOMOD
	menuSections[SC_FAV]->AddTool(new FavTool(0));
#endif
	std::vector<std::string> favorites = Favorite::Ref().BuildFavoritesList();
	for (std::vector<std::string>::iterator iter = favorites.begin(); iter != favorites.end(); ++iter)
	{
		Tool *tool = GetToolFromIdentifier((*iter));
		if (tool)
			menuSections[SC_FAV]->AddTool(tool);
	}
#ifndef NOMOD
	for (int i = 1; i < NUM_FAV_BUTTONS; i++)
	{
		menuSections[SC_FAV2]->AddTool(new FavTool(i));
	}
	for (int i = 0; i < HUD_NUM; i++)
	{
		menuSections[SC_HUD]->AddTool(new HudTool(i));
	}
#endif

	for (const auto &luaTool : luaTools)
	{
		// Create lua tool object from metadata
		LuaTool *tool = luaTool.second.GetTool();
		if (tool->GetMenuVisible() && tool->GetMenuSection() >= 0 && tool->GetMenuSection() < SC_TOTAL)
			menuSections[tool->GetMenuSection()]->AddTool(tool);
		else
			menuSections[SC_OTHER]->AddTool(tool);
	}

	//restore active tools
	if (activeTools[0])
	{
		for (int i = 0; i < 3; i++)
		{
			Tool* temp = GetToolFromIdentifier(tempActiveTools[i]);
			if (temp == nullptr)
				temp = GetToolFromIdentifier("DEFAULT_PT_NONE");
			activeTools[i] = temp;
		}
	}
}
