/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <iostream>

//Simulation stuff
#include "Simulation.h"
#include "CoordStack.h"
#include "interface.h" //for framenum, try to remove this later
#include "luaconsole.h" //for lua_el_mode
#include "misc.h"
#include "powder.h"
#include "Element.h"
#include "ElementDataContainer.h"
#include "Tool.h"

#include "common/Format.h"
#include "common/tpt-math.h"
#include "common/tpt-minmax.h"
#include "common/tpt-rand.h"
#include "game/Brush.h"
#include "game/Menus.h" // for active_menu setting on save load, try to remove this later
#include "common/Translations.h"
#include "game/Save.h"
#include "game/Sign.h"
#include "graphics/Renderer.h"
#include "simulation/elements/ANIM.h"
#include "simulation/elements/LIFE.h"
#include "simulation/elements/MOVS.h"
#include "simulation/elements/FIGH.h"
#include "simulation/elements/PLNT.h"
#include "simulation/elements/PPIP.h"
#include "simulation/elements/STKM.h"

// Declare the element initialisation functions
#define ElementNumbers_Include_Decl
#define DEFINE_ELEMENT(name, id) void name ## _init_element(ELEMENT_INIT_FUNC_ARGS);
#include "ElementNumbers.h"
#include "GolNumbers.h"
#include "ToolNumbers.h"
#include "WallNumbers.h"


Simulation *globalSim = NULL; // TODO: remove this global variable

Simulation::Simulation():
	currentTick(0),
	pfree(-1),
	parts_lastActiveIndex(NPART-1),
	debug_currentParticle(0),
	forceStackingCheck(false),
	msRotation(true),
#ifdef NOMOD
	instantActivation(false),
#else
	instantActivation(true),
#endif
	lightningRecreate(0)
{
	std::fill(&elementData[0], &elementData[PT_NUM], nullptr);

	// Initialize global parts variable. TODO: make everything use sim->parts
	::parts = this->parts;

	air = new Air();
	grav = new Gravity();

	Clear();
	InitElements();
	std::copy(&elements[0], &elements[PT_NUM], &origElements[0]);
	InitCanMove();
}

Simulation::~Simulation()
{
	delete air;
	delete grav;
}

void Simulation::InitElements()
{
	#define DEFINE_ELEMENT(name, id) if (id>=0 && id<PT_NUM) { name ## _init_element(this, &elements[id], id); };
	#define ElementNumbers_Include_Call
	#include "simulation/ElementNumbers.h"
}

void Simulation::Clear()
{
	air->Clear();
	grav->Clear();
	for (int t = 0; t < PT_NUM; t++)
	{
		if (elementData[t])
		{
			elementData[t]->Simulation_Cleared(this);
		}
	}
	std::fill(&elementCount[0], &elementCount[PT_NUM], 0);
	pfree = 0;
	NUM_PARTS = 0;
	parts_lastActiveIndex = NPART-1;

#ifdef NOMOD
	instantActivation = false;
#else
	instantActivation = true;
#endif
	saveEdgeMode = -1;
	if (edgeMode == EDGE_SOLID)
		draw_bframe();
}

CoordStack& Simulation::getCoordStackSingleton()
{
	// Future-proofing in case Simulation is later multithreaded
	thread_local CoordStack cs;
	cs.clear();
	return cs;
}

void Simulation::RecountElements()
{
	std::fill(&elementCount[0], &elementCount[PT_NUM], 0);
	for (int i = 0; i < NPART; i++)
		if (parts[i].type)
			elementCount[parts[i].type]++;
}

SaveLoadData Simulation::LoadSave(int loadX, int loadY, const Save *originalSave, int replace, bool includePressure)
{
	if (!originalSave)
		return {};
	auto save = std::unique_ptr<Save>(new Save(*originalSave));
	if (!save->expanded)
		save->ParseSave();

	// align to blockMap
	int blockX = (loadX + CELL/2)/CELL;
	int blockY = (loadY + CELL/2)/CELL;
	loadX = blockX*CELL;
	loadY = blockY*CELL;
	unsigned int pmapmask = (1 << save->pmapbits) - 1;

	if (replace >= 1)
	{
		if (save->ambientAirTempPresent)
			air->SetAmbientAirTemp(save->ambientAirTemp);
		//if (save->edgePressurePresent)
		air->SetEdgePressure(save->edgePressure);
		//if (save->edgeVelocityPresent)
		air->SetEdgeVelocity(save->edgeVelocityX, save->edgeVelocityY);
		clear_sim();
		erase_bframe();
		instantActivation = false;
	}

	bool hasPalette = false;
	int partMap[PT_NUM];
	bool ignoreMissingErrors[PT_NUM];
	std::map<std::string, int> missingElementIdentifiers;
	for (int i = 0; i < PT_NUM; i++)
	{
		partMap[i] = i;
		ignoreMissingErrors[i] = false;
	}
	if (save->createdVersion < 99)
	{
		ignoreMissingErrors[PT_ICEI] = true;
		ignoreMissingErrors[PT_SNOW] = true;
		ignoreMissingErrors[PT_RSST] = true;
		ignoreMissingErrors[PT_RSSS] = true;
	}
	if (save->createdVersion < 100)
	{
		ignoreMissingErrors[PT_PLSM] = true;
		ignoreMissingErrors[PT_EXOT] = true;
		ignoreMissingErrors[PT_FWRK] = true;
		ignoreMissingErrors[PT_WTRV] = true;
		ignoreMissingErrors[PT_FIRE] = true;
		ignoreMissingErrors[PT_BRMT] = true;
	}

	SaveLoadData saveLoadData;
	auto &possiblyCarriesType = particle::PossiblyCarriesType();
	auto &properties = particle::GetProperties();

	if (save->palette.size())
	{
		if (save->createdVersion >= 98)
		{
			for(int i = 0; i < PT_NUM; i++)
			{
				partMap[i] = 0;
			}
		}
		for (auto &pi : save->palette)
		{
			if (pi.second <= 0 || pi.second >= PT_NUM)
				continue;
			int myId = 0;
			for (int i = 0; i < PT_NUM; i++)
			{
				if (elements[i].Enabled && elements[i].Identifier == pi.first)
				{
					myId = i;
				}
			}
			if (myId)
			{
				partMap[pi.second] = myId;
			}
			else
			{
				missingElementIdentifiers.insert(pi);
			}
		}
		hasPalette = true;
	}
	auto paletteLookup = [&partMap, &saveLoadData](int type, bool ignoreMissingErrors) {
		if (type > 0 && type < PT_NUM)
		{
			auto carriedType = partMap[type];
			if (!carriedType) // type is not 0 so this shouldn't be 0 either
			{
				if (ignoreMissingErrors)
					return type;
				saveLoadData.ids.insert(type);
			}
			type = carriedType;
		}
		return type;
	};

#ifndef NOMOD
	// Solids is a map of the old .tmp2 it was saved with, to the new ball number it is getting
	int solids[MAX_MOVING_SOLIDS];
	// Default to invalid ball
	std::fill(&solids[0], &solids[MAX_MOVING_SOLIDS], MAX_MOVING_SOLIDS);
	if (save->MOVSdata.size())
	{
		int numBalls = static_cast<MOVS_ElementDataContainer&>(*elementData[PT_MOVS]).GetNumBalls();
		for (auto &data : save->MOVSdata)
		{
			int bn = data.first;
			if (bn >= 0 && bn < MAX_MOVING_SOLIDS)
			{
				solids[bn] = numBalls;
				MovingSolid *movingSolid = static_cast<MOVS_ElementDataContainer&>(*elementData[PT_MOVS]).GetMovingSolid(numBalls++);
				// Create a moving solid and clear all its variables
				if (movingSolid)
				{
					movingSolid->Simulation_Cleared();
					// Set its rotation
					movingSolid->rotationOld = movingSolid->rotation = data.second/20.0f - 2*M_PI;
				}
			}
		}
		// New number of known moving solids
		static_cast<MOVS_ElementDataContainer&>(*elementData[PT_MOVS]).SetNumBalls(numBalls);
	}

	unsigned int animDataPos = 0;
#endif

	int r;
	bool doFullScan = false;
	for (unsigned int n = 0; n < NPART && n < save->particlesCount; n++)
	{
		particle &tempPart = save->particles[n]; // TODO should be & not *, see GameSave::MapPalette
		tempPart.x += (float)loadX;
		tempPart.y += (float)loadY;
		int x = int(std::floor(tempPart.x + 0.5f));
		int y = int(std::floor(tempPart.y + 0.5f));
		
		auto &type = tempPart.type;

		// Check various scenarios where we are unable to spawn the element, and set type to 0 to block spawning later
		if (!InBounds(x, y))
		{
			tempPart.type = 0;
			continue;
		}

		if (type < 0 && type >= PT_NUM)
		{
			type = 0;
			continue;
		}

		// Convert element type according to palette
		if (type > 0 && type < PT_NUM)
		{
			if (hasPalette)
				type = paletteLookup(type, false);
			else
				type = save->FixType(type);
		}
		else
			continue;

		// Convert elements stored in other properties according to palette as well
		for (auto index : possiblyCarriesType)
		{
			if (elements[type].CarriesTypeIn & (1U << index))
			{
				auto *prop = reinterpret_cast<int *>(reinterpret_cast<char *>(&tempPart) + properties[index].Offset);
				int carriedType = *prop & int(pmapmask);
				int extra = *prop >> save->pmapbits;
				if (hasPalette)
					carriedType = paletteLookup(carriedType, ignoreMissingErrors[tempPart.type]);
				else
					carriedType = save->FixType(carriedType);
				*prop = PMAP(extra, carriedType);
			}
		}

		// Ensure we can spawn this element
		if ((type == PT_STKM || type == PT_STKM2 || type == PT_SPAWN || type == PT_SPAWN2) && elementCount[type] > 0)
		{
			type = 0;
			continue;
		}
		if (type == PT_FIGH && !static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).CanAlloc())
		{
			type = 0;
			continue;
		}
		if (!elements[type].Enabled)
		{
			type = 0;
			continue;
		}

		if ((r = pmap[y][x]))
		{
			// Particle already exists in this location. Set pmap to 0, then kill it and all stacked particles in the loop below
			pmap[y][x] = 0;
			doFullScan = true;
		}
		else if ((r = photons[y][x]))
		{
			// Particle already exists in this location. Set photons to 0, then kill it and all stacked particles in the loop below
			photons[y][x] = 0;
			doFullScan = true;
		}
	}

	for (const auto &pi : missingElementIdentifiers)
	{
		if (saveLoadData.ids.find(pi.second) != saveLoadData.ids.end())
		{
			saveLoadData.identifiers.insert(pi);
		}
	}

	if (doFullScan)
	{
		// Loop through particles to find particles in need of being killed
		for (int i = 0; i <= parts_lastActiveIndex; i++) {
			if (parts[i].type)
			{
				int x = int(parts[i].x + 0.5f);
				int y = int(parts[i].y + 0.5f);
				if (elements[parts[i].type].Properties & TYPE_ENERGY)
				{
					if (!photons[y][x])
						part_kill(i);
				}
				else
				{
					if (!pmap[y][x])
						part_kill(i);
				}
			}
		}
	}

	int i;
	// Map of soap particles loaded into this save, old ID -> new ID
	std::map<unsigned int, unsigned int> soapList;
	for (unsigned int n = 0; n < NPART && n < save->particlesCount; n++)
	{
		particle tempPart = save->particles[n];
		if (tempPart.type == 0)
			continue;

		if (elements[tempPart.type].Func_Create_Allowed)
		{
			if (!(*(elements[tempPart.type].Func_Create_Allowed))(this, -3, int(tempPart.x + 0.5f), int(tempPart.y + 0.5f), tempPart.type))
			{
				continue;
			}
		}

		if (save->legacyHeatSave)
		{
			tempPart.temp = elements[tempPart.type].DefaultProperties.temp;
		}

		// Allocate particle (this location is guaranteed to be empty due to "full scan" logic above)
		i = part_alloc();
		parts[i] = tempPart;
		elementCount[tempPart.type]++;

		switch (parts[i].type)
		{
		case PT_STKM:
		{
			bool fan = false;
			if ((save->createdVersion < 93 && parts[i].ctype == SPC_AIR)
			        || (save->createdVersion < 88 && parts[i].ctype == OLD_SPC_AIR))
			{
				fan = true;
				parts[i].ctype = 0;
			}
			static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).NewStickman1(i, parts[i].ctype);
			if (fan)
				static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman1()->fan = true;
			if (save->stkm.rocketBoots1)
				static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman1()->rocketBoots = true;
			if (save->stkm.fan1)
				static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman1()->fan = true;
			break;
		}
		case PT_STKM2:
		{
			bool fan = false;
			if ((save->createdVersion < 93 && parts[i].ctype == SPC_AIR)
			        || (save->createdVersion < 88 && parts[i].ctype == OLD_SPC_AIR))
			{
				fan = true;
				parts[i].ctype = 0;
			}
			static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).NewStickman2(i, parts[i].ctype);
			if (fan)
				static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman2()->fan = true;
			if (save->stkm.rocketBoots2)
				static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman2()->rocketBoots = true;
			if (save->stkm.fan2)
				static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman2()->fan = true;
		}
		case PT_SPAWN:
			static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman1()->spawnID = i;
			break;
		case PT_SPAWN2:
			static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman2()->spawnID = i;
			break;
		case PT_FIGH:
		{
			unsigned int oldTmp = parts[i].tmp;
			parts[i].tmp = static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).Alloc();
			if (parts[i].tmp >= 0)
			{
				bool fan = false;
				if ((save->createdVersion < 93 && parts[i].ctype == SPC_AIR)
				        || (save->createdVersion < 88 && parts[i].ctype == OLD_SPC_AIR))
				{
					fan = true;
					parts[i].ctype = 0;
				}
				static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).NewFighter(this, parts[i].tmp, i, parts[i].ctype);
				if (fan)
					static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).Get(parts[i].tmp)->fan = true;
				for (unsigned int fighNum : save->stkm.rocketBootsFigh)
				{
					if (fighNum == oldTmp)
						static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).Get(parts[i].tmp)->rocketBoots = true;
				}
				for (unsigned int fighNum : save->stkm.fanFigh)
				{
					if (fighNum == oldTmp)
						static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).Get(parts[i].tmp)->fan = true;
				}
			}
			else
				// Should not be possible because we verify with CanAlloc above this
				parts[i].type = PT_NONE;
			break;
		}
		case PT_SOAP:
			soapList.insert(std::pair<unsigned int, unsigned int>(n, i));
			break;
#ifndef NOMOD
		// special handling for MOVS: ensure it is valid, and fix issues with signed values in tmp3/tmp4
		case PT_MOVS:
			if ((parts[i].flags&FLAG_DISAPPEAR) || parts[i].tmp2 < 0 || parts[i].tmp2 >= MAX_MOVING_SOLIDS)
				parts[i].tmp2 = MAX_MOVING_SOLIDS;
			else
			{
				parts[i].tmp2 = solids[parts[i].tmp2];
				MovingSolid *movingSolid = static_cast<MOVS_ElementDataContainer&>(*elementData[PT_MOVS]).GetMovingSolid(parts[i].tmp2);
				if (movingSolid)
				{
					// Increase ball particle count
					movingSolid->particleCount++;
					// Set center "controlling" particle
					if (parts[i].tmp3 == 0 && parts[i].tmp4 == 0)
						movingSolid->index = i+1;
				}
			}

			if (parts[i].tmp3 > 32768)
				parts[i].tmp3 -= 65536;
			if (parts[i].tmp4 > 32768)
				parts[i].tmp4 -= 65536;
			break;
		case PT_ANIM:
			if (animDataPos >= save->ANIMdata.size())
			{
				parts[i].type = PT_NONE;
				continue;
			}

			Save::ANIMdataItem data =  save->ANIMdata[animDataPos++];
			static_cast<ANIM_ElementDataContainer&>(*elementData[PT_ANIM]).SetAllColors(i, data.second, data.first + 1);
			break;
#endif
		}

		if (Save::PressureInTmp3(parts[i].type) && !includePressure)
		{
			parts[i].tmp3 = 0;
		}
	}

	// fix SOAP links using soapList, a map of old particle ID -> new particle ID
	// loop through every old particle (loaded from save), and convert .tmp / .tmp2
	for (std::map<unsigned int, unsigned int>::iterator iter = soapList.begin(), end = soapList.end(); iter != end; ++iter)
	{
		int i = (*iter).second;
		if ((parts[i].ctype & 0x2) == 2)
		{
			auto n = soapList.find(parts[i].tmp);
			if (n != end)
				parts[i].tmp = n->second;
			// sometimes the proper SOAP isn't found. It should remove the link, but seems to break some saves
			// so just ignore it
		}
		if ((parts[i].ctype & 0x4) == 4)
		{
			auto n = soapList.find(parts[i].tmp2);
			if (n != end)
				parts[i].tmp2 = n->second;
			// sometimes the proper SOAP isn't found. It should remove the link, but seems to break some saves
			// so just ignore it
		}
	}

	for (size_t i = 0; i < save->signs.size() && signs.size() < MAXSIGNS; i++)
	{
		if (save->signs[i].GetText().length())
		{
			Sign tempSign = save->signs[i];
			Point pos = tempSign.GetRealPos() + Point(loadX, loadY);
			if (!InBounds(pos.X, pos.Y))
				continue;
			tempSign.SetPos(pos);
			signs.push_back(tempSign);
		}
	}
	for (unsigned int saveBlockX = 0; saveBlockX < save->blockWidth; saveBlockX++)
	{
		for (unsigned int saveBlockY = 0; saveBlockY < save->blockHeight; saveBlockY++)
		{
			if (!InBounds((saveBlockX+blockX)*CELL, (saveBlockY+blockY)*CELL))
				continue;

			if (save->blockMap[saveBlockY][saveBlockX])
			{
				bmap[saveBlockY+blockY][saveBlockX+blockX] = save->blockMap[saveBlockY][saveBlockX];
				air->fvx[saveBlockY+blockY][saveBlockX+blockX] = save->fanVelX[saveBlockY][saveBlockX];
				air->fvy[saveBlockY+blockY][saveBlockX+blockX] = save->fanVelY[saveBlockY][saveBlockX];
			}
			if (includePressure)
			{
				if (save->hasPressure)
				{
					air->pv[saveBlockY+blockY][saveBlockX+blockX] = save->pressure[saveBlockY][saveBlockX];
					air->vx[saveBlockY+blockY][saveBlockX+blockX] = save->velocityX[saveBlockY][saveBlockX];
					air->vy[saveBlockY+blockY][saveBlockX+blockX] = save->velocityY[saveBlockY][saveBlockX];
				}
				if (save->hasAmbientHeat)
					air->hv[saveBlockY+blockY][saveBlockX+blockX] = save->ambientHeat[saveBlockY][saveBlockX];
			}
		}
	}

	// check for excessive stacking of particles next time update_particles is run
	forceStackingCheck = 1;
	grav->gravWallChanged = true;
	static_cast<PPIP_ElementDataContainer&>(*elementData[PT_PPIP]).ppip_changed = 1;
	air->RecalculateBlockAirMaps(this);
	RecalcFreeParticles(false);

	if (save->paused)
		sys_pause = true;
	if (replace >= 1)
	{
		legacy_enable = save->legacyEnable;
		aheat_enable = save->aheatEnable;
		water_equal_test = save->waterEEnabled;
		if (!sys_pause || replace == 2)
			sys_pause = save->paused;
		air->airMode = save->airMode;
		//if (save->ambientAirTempPresent)
		//	air->SetAmbientAirTemp(save->ambientAirTemp);
		air->SetVorticityCoeff(save->vorticityCoeff);
		int convectionMode = save->convectionMode;
		if (!save->convectionModePresent && save->createdVersion >= 99)
			convectionMode = AIRC_BOUSSINESQ;
		air->SetTempConvectionMode(convectionMode);
		gravityMode = save->gravityMode;
		customGravityX = save->customGravityX;
		customGravityY = save->customGravityY;
		saveEdgeMode = save->edgeMode;
		if (save->msRotationPresent)
			msRotation = save->msRotation;
#ifndef NOMOD
		if (save->modCreatedVersion)
			instantActivation = true;
#endif

		// loading a tab, replace even more simulation / UI options
		if (replace >= 2)
		{
#ifndef TOUCHUI
			if (save->hudEnablePresent)
				hud_enable = save->hudEnable;
#endif
			if (save->activeMenuPresent && save->activeMenu >= 0 && save->activeMenu < SC_TOTAL && menuSections[save->activeMenu]->enabled)
				active_menu = save->activeMenu;
			if (save->decorationsEnablePresent)
				decorations_enable = save->decorationsEnable;

			if (save->leftSelectedIdentifier.length())
			{
				Tool *temp = GetToolFromIdentifier(save->leftSelectedIdentifier);
				if (temp)
					activeTools[0] = temp;
			}
			if (save->rightSelectedIdentifier.length())
			{
				Tool *temp = GetToolFromIdentifier(save->rightSelectedIdentifier);
				if (temp)
					activeTools[1] = temp;
			}

			if (save->saveInfoPresent)
			{
				// TODO: use SaveInfo for this globally too
				svf_open = save->saveInfo.GetSaveOpened();
				svf_fileopen = save->saveInfo.GetFileOpened();
				strncpy(svf_name, save->saveInfo.GetSaveName().c_str(), 63);
				strncpy(svf_filename, save->saveInfo.GetFileName().c_str(), 254);
				svf_publish = save->saveInfo.GetPublished();
				snprintf(svf_id, 15, "%i", save->saveInfo.GetSaveID());
				svf_version = save->saveInfo.GetSaveVersion();
				strncpy(svf_description, save->saveInfo.GetDescription().c_str(), 254);
				strncpy(svf_author, save->saveInfo.GetAuthor().c_str(), 63);
				strncpy(svf_tags, save->saveInfo.GetTags().c_str(), 255);
				svf_myvote = save->saveInfo.GetMyVote();

				svf_own = svf_login && !strcmp(svf_author, svf_user);
				svf_publish = svf_publish && svf_own;
			}
		}

#ifndef RENDERER
		// Start / stop gravity thread
		if (grav->IsEnabled() != save->gravityEnable)
		{
			if (save->gravityEnable)
				grav->StartAsync();
			else
				grav->StopAsync();
		}
#endif

#ifdef LUACONSOLE
		if (save->luaCode.length())
		{
			LuaCode = mystrdup(save->luaCode.c_str());
			LuaCodeLen = save->luaCode.size();
			ranLuaCode = false;
		}
#endif
	}

#ifdef LUACONSOLE
	for (auto logMessage : save->logMessages)
		luacon_log(logMessage);

	if (!strcmp(svf_user, "jacob1") && replace == 1)
	{
		for (auto adminLogMessage : save->adminLogMessages)
			luacon_log(adminLogMessage);
	}
#endif

	Renderer::Ref().LoadSave(save.get());
	saveLoadData.authors = save->authors;
	return saveLoadData;
}

Save * Simulation::CreateSave(int fullX, int fullY, int fullX2, int fullY2, bool includePressure)
{
	// Normalise incoming coords
	int swapTemp;
	if (fullY > fullY2)
	{
		swapTemp = fullY;
		fullY = fullY2;
		fullY2 = swapTemp;
	}
	if (fullX > fullX2)
	{
		swapTemp = fullX;
		fullX = fullX2;
		fullX2 = swapTemp;
	}

	//Align coords to blockMap
	int blockX = fullX / CELL;
	int blockY = fullY / CELL;

	int blockX2 = (fullX2+CELL-1) / CELL;
	int blockY2 = (fullY2+CELL-1) / CELL;

	int blockW = blockX2 - blockX;
	int blockH = blockY2 - blockY;

	Save * newSave = new Save(blockW, blockH);

	// Palette stuff
	newSave->SetSim(this);
	auto &possiblyCarriesType = particle::PossiblyCarriesType();
	auto &properties = particle::GetProperties();

	includePressure ^= !this->includePressure;
	
	int storedParts = 0;
	int elementCount[PT_NUM];
	std::fill(elementCount, elementCount+PT_NUM, 0);

	// Map of soap particles loaded into this save, new ID -> old ID
	// Now stores all particles, not just SOAP (but still only used for soap)
	std::map<unsigned int, unsigned int> particleMap;
#ifndef NOMOD
	// Stores list of moving solids in the save area
	bool solids[MAX_MOVING_SOLIDS];
	std::fill(&solids[0], &solids[MAX_MOVING_SOLIDS], false);
#endif
	std::set<int> paletteSet;
	for (int i = 0; i < NPART; i++)
	{
		int x, y;
		x = int(parts[i].x + 0.5f);
		y = int(parts[i].y + 0.5f);
		if (parts[i].type && x >= fullX && y >= fullY && x < fullX2 && y < fullY2)
		{
			particle tempPart = parts[i];
			tempPart.x -= blockX*CELL;
			tempPart.y -= blockY*CELL;

			if (elements[tempPart.type].Enabled)
			{
				particleMap.insert(std::pair<unsigned int, unsigned int>(i, storedParts));
#ifndef NOMOD
				if (tempPart.type == PT_MOVS)
				{
					if (tempPart.tmp2 >= 0 && tempPart.tmp2 < MAX_MOVING_SOLIDS)
						solids[tempPart.tmp2] = true;
				}
				else if (tempPart.type == PT_ANIM)
				{
					int animLength = std::min(tempPart.ctype, (int)(static_cast<ANIM_ElementDataContainer&>(*elementData[PT_ANIM]).GetMaxFrames() - 1));
					Save::ANIMdataItem data;
					data.first = animLength;
					data.second = static_cast<ANIM_ElementDataContainer&>(*elementData[PT_ANIM]).GetAllColors(i, tempPart.ctype+1);
					newSave->ANIMdata.push_back(data);
				}
#endif
				*newSave << tempPart;
				storedParts++;
				elementCount[tempPart.type]++;

				paletteSet.insert(tempPart.type);
				for (auto index : possiblyCarriesType)
				{
					if (elements[tempPart.type].CarriesTypeIn & (1U << index))
					{
						auto *prop = reinterpret_cast<const int *>(reinterpret_cast<const char *>(&tempPart) + properties[index].Offset);
						paletteSet.insert(TYP(*prop));
					}
				}
			}
		}
	}
	for (int ID : paletteSet)
		newSave->palette.push_back(Save::PaletteItem(elements[ID].Identifier, ID));

	if (storedParts && elementCount[PT_SOAP])
	{
		// fix SOAP links using particleMap, a map of old particle ID -> new particle ID
		// loop through every new particle (saved into the save), and convert .tmp / .tmp2
		for (std::map<unsigned int, unsigned int>::iterator iter = particleMap.begin(), end = particleMap.end(); iter != end; ++iter)
		{
			int i = (*iter).second;
			if (newSave->particles[i].type != PT_SOAP)
				continue;
			if ((newSave->particles[i].ctype & 0x2) == 2)
			{
				std::map<unsigned int, unsigned int>::iterator n = particleMap.find(newSave->particles[i].tmp);
				if (n != end)
					newSave->particles[i].tmp = n->second;
				else
				{
					newSave->particles[i].tmp = 0;
					newSave->particles[i].ctype ^= 2;
				}
			}
			if ((newSave->particles[i].ctype & 0x4) == 4)
			{
				std::map<unsigned int, unsigned int>::iterator n = particleMap.find(newSave->particles[i].tmp2);
				if (n != end)
					newSave->particles[i].tmp2 = n->second;
				else
				{
					newSave->particles[i].tmp2 = 0;
					newSave->particles[i].ctype ^= 4;
				}
			}
		}
	}

	for (size_t i = 0; i < MAXSIGNS && i < signs.size(); i++)
	{
		if (signs[i].GetText().length() && signs[i].IsSignInArea(Point(fullX, fullY), Point(fullX2, fullY2)))
		{
			Sign tempSign = Sign(signs[i]);
			tempSign.SetPos(tempSign.GetRealPos() - Point(blockX*CELL, blockY*CELL));
			*newSave << tempSign;
		}
	}
	
	for (unsigned int saveBlockX = 0; saveBlockX < newSave->blockWidth; saveBlockX++)
	{
		for (unsigned int saveBlockY = 0; saveBlockY < newSave->blockHeight; saveBlockY++)
		{
			if (bmap[saveBlockY+blockY][saveBlockX+blockX])
			{
				newSave->blockMap[saveBlockY][saveBlockX] = bmap[saveBlockY+blockY][saveBlockX+blockX];
				newSave->fanVelX[saveBlockY][saveBlockX] = air->fvx[saveBlockY+blockY][saveBlockX+blockX];
				newSave->fanVelY[saveBlockY][saveBlockX] = air->fvy[saveBlockY+blockY][saveBlockX+blockX];
			}
			if (includePressure)
			{
				newSave->pressure[saveBlockY][saveBlockX] = air->pv[saveBlockY+blockY][saveBlockX+blockX];
				newSave->velocityX[saveBlockY][saveBlockX] = air->vx[saveBlockY+blockY][saveBlockX+blockX];
				newSave->velocityY[saveBlockY][saveBlockX] = air->vy[saveBlockY+blockY][saveBlockX+blockX];
				newSave->hasPressure = true;
				if (aheat_enable)
				{
					newSave->ambientHeat[saveBlockY][saveBlockX] = air->hv[saveBlockY+blockY][saveBlockX+blockX];
					newSave->hasAmbientHeat = true;
				}
			}
		}
	}

	Stickman *stkm = static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman1();
	newSave->stkm.rocketBoots1 = stkm->rocketBoots;
	newSave->stkm.fan1 = stkm->fan;
	stkm = static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman2();
	newSave->stkm.rocketBoots2 = stkm->rocketBoots;
	newSave->stkm.fan2 = stkm->fan;

	for (unsigned char i = 0; i < static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).MaxFighters(); i++)
	{
		stkm = static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).Get(i);
		if (stkm->rocketBoots)
			newSave->stkm.rocketBootsFigh.push_back(i);
		if (stkm->fan)
			newSave->stkm.fanFigh.push_back(i);
	}

#ifndef NOMOD
	for (unsigned int bn = 0; bn < MAX_MOVING_SOLIDS; bn++)
		// List of moving solids that are in the save area, filled above
		if (solids[bn])
		{
			MovingSolid* movingSolid = static_cast<MOVS_ElementDataContainer&>(*elementData[PT_MOVS]).GetMovingSolid(bn);
			if (movingSolid && (movingSolid->index || movingSolid->particleCount))
			{
				newSave->MOVSdata.push_back(Save::MOVSdataItem(bn, (char)((movingSolid->rotationOld + 2*M_PI)*20)));
			}
		}
#endif
	newSave->paused = sys_pause;
	newSave->gravityMode = gravityMode;
	newSave->customGravityX = customGravityX;
	newSave->customGravityY = customGravityY;
	newSave->airMode = air->airMode;
	newSave->ambientAirTemp = air->GetAmbientAirTemp();
	newSave->edgePressure = air->GetEdgePressure();
	newSave->edgeVelocityX = air->GetEdgeVelocityX();
	newSave->edgeVelocityY = air->GetEdgeVelocityY();
	newSave->vorticityCoeff = air->GetVorticityCoeff();
	newSave->convectionMode = air->GetConvectionMode();
	newSave->edgeMode = edgeMode;
	newSave->legacyEnable = legacy_enable;
	newSave->waterEEnabled = water_equal_test;
	newSave->gravityEnable = grav->IsEnabled();
	newSave->aheatEnable = aheat_enable;

	// Mod settings
	newSave->msRotation = msRotation;
	newSave->msRotationPresent = true;
	newSave->leftSelectedIdentifier = activeTools[0]->GetIdentifier().c_str();
	newSave->rightSelectedIdentifier = activeTools[1]->GetIdentifier().c_str();

	newSave->saveInfo.SetSaveOpened(svf_open);
	newSave->saveInfo.SetFileOpened(svf_fileopen);
	newSave->saveInfo.SetSaveName(svf_name);
	newSave->saveInfo.SetFileName(svf_filename);
	newSave->saveInfo.SetPublished(svf_publish);
	newSave->saveInfo.SetSaveID(Format::StringToNumber<int>(svf_id));
	newSave->saveInfo.SetSaveVersion(svf_version);
	newSave->saveInfo.SetDescription(svf_description);
	newSave->saveInfo.SetAuthor(svf_author);
	newSave->saveInfo.SetTags(svf_tags);
	newSave->saveInfo.SetMyVote(svf_myvote);
	newSave->saveInfoPresent = true;

	Renderer::Ref().CreateSave(newSave);

#ifdef LUACONSOLE
	if (LuaCode)
		newSave->luaCode = LuaCode;
#endif

	newSave->expanded = true;
	newSave->pmapbits = PMAPBITS;
	return newSave;
}

// the function for creating a particle
// p=-1 for normal creation (checks whether the particle is allowed to be in that location first)
// p=-3 to create without checking whether the particle is allowed to be in that location
// or p = a particle number, to replace a particle
int Simulation::part_create(int p, int x, int y, int t, int v)
{
	// This function is only for actually creating particles.
	// Not for tools, or changing things into spark, or special brush things like setting clone ctype.

	int i, oldType = PT_NONE;
	if (x<0 || y<0 || x>=XRES || y>=YRES || t<=0 || t>=PT_NUM || !elements[t].Enabled)
	{
		return -1;
	}

	// Spark Checks here
	if (t == PT_SPRK && !(p == -2 && elements[TYP(pmap[y][x])].CtypeDraw))
	{
		int type = TYP(pmap[y][x]);
		int index = ID(pmap[y][x]);
		if(type == PT_WIRE)
		{
			parts[index].ctype = PT_DUST;
			return index;
		}
		if (!(type == PT_INST || (elements[type].Properties&PROP_CONDUCTS)))
			return -1;
		if (parts[index].life!=0)
			return -1;
		if (type == PT_INST)
		{
			if (p == -2)
				INST_flood_spark(this, x, y);
			else
				spark_conductive(index, x, y);
			return index;
		}

		spark_conductive_attempt(index, x, y);
		return index;
	}
	// End Spark checks

	// Brush Creation
	if (p == -2)
	{
		if (pmap[y][x])
		{
			int drawOn = TYP(pmap[y][x]);
			if (elements[drawOn].CtypeDraw)
				elements[drawOn].CtypeDraw(this, ID(pmap[y][x]), t, v);
			return -1;
		}
		else if (IsWallBlocking(x, y, t))
			return -1;
		else if (photons[y][x] && (elements[t].Properties & TYPE_ENERGY))
			return -1;
	}
	// End Brush Creation

	if (elements[t].Func_Create_Allowed)
	{
		if (!(*(elements[t].Func_Create_Allowed))(this, p, x, y, t))
			return -1;
	}

	if (p == -1)
	{
		// Check whether the particle can be created here

		// If there is a particle, only allow creation if the new particle can occupy the same space as the existing particle
		// If there isn't a particle but there is a wall, check whether the new particle is allowed to be in it
		//   (not "!=2" for wall check because eval_move returns 1 for moving into empty space)
		// If there's no particle and no wall, assume creation is allowed
		if (pmap[y][x] ? (EvalMove(t, x, y) != 2) : (bmap[y/CELL][x/CELL] && EvalMove(t, x, y) == 0))
		{
			return -1;
		}
		i = part_alloc();
	}
	else if (p == -3) // skip pmap checks, e.g. for sing explosion
	{
		i = part_alloc();
	}
	else if (p >= 0) // Replace existing particle
	{
		int oldX = (int)(parts[p].x+0.5f);
		int oldY = (int)(parts[p].y+0.5f);
		oldType = parts[p].type;
		if (elements[oldType].Func_ChangeType)
		{
			(*(elements[oldType].Func_ChangeType))(this, p, oldX, oldY, oldType, t);
		}
		if (oldType) elementCount[oldType]--;
		pmap_remove(p, oldX, oldY);
		i = p;
	}
	else // Dunno, act like it was p=-3
	{
		i = part_alloc();
	}

	// Check whether a particle was successfully allocated
	if (i<0)
		return -1;

	// Set some properties
	parts[i] = elements[t].DefaultProperties;
	parts[i].type = t;
	parts[i].x = (float)x;
	parts[i].y = (float)y;

	// Fancy dust effects for powder types
	if ((elements[t].Properties & TYPE_PART) && pretty_powder)
	{
		int sandcolor = (int)(20.0f*sin((float)(currentTick%360)*(M_PI/180.0f)));
		int colr = COLR(elements[t].Colour) + (int)(sandcolor * 1.3f) + RNG::Ref().between(-20, 20) + RNG::Ref().between(-15, 15);
		int colg = COLG(elements[t].Colour) + (int)(sandcolor * 1.3f) + RNG::Ref().between(-20, 20) + RNG::Ref().between(-15, 15);
		int colb = COLB(elements[t].Colour) + (int)(sandcolor * 1.3f) + RNG::Ref().between(-20, 20) + RNG::Ref().between(-15, 15);
		colr = std::max(0, std::min(255, colr));
		colg = std::max(0, std::min(255, colg));
		colb = std::max(0, std::min(255, colb));
		parts[i].dcolour = COLARGB(RNG::Ref().between(0, 149), colr, colg, colb);
	}

	// Set non-static properties (such as randomly generated ones)
	if (elements[t].Func_Create)
	{
		(*(elements[t].Func_Create))(this, i, x, y, t, v);
	}

	pmap_add(i, x, y, t);

	if (elements[t].Func_ChangeType)
	{
		(*(elements[t].Func_ChangeType))(this, i, x, y, oldType, t);
	}

	elementCount[t]++;
	return i;
}

int Simulation::part_create_preserve_energy(int i, int x, int y, int t)
{
	float temp = parts[i].temp;
	float vx = parts[i].vx;
	float vy = parts[i].vy;

	int np = part_create(i, x, y, t);
	if (np >= 0)
	{
		parts[np].temp = temp;
		parts[np].vx = vx;
		parts[np].vy = vy;
	}

	return np;
}

// changes the type of particle number i, to t. This also changes pmap at the same time.
bool Simulation::part_change_type(int i, int x, int y, int t, bool ignore_indestructible)
{
	if (x<0 || y<0 || x>=XRES || y>=YRES || i>=NPART || t<0 || t>=PT_NUM)
		return false;

	if (t == parts[i].type)
		return true;
	if (!elements[t].Enabled)
	{
		part_kill(i);
		return true;
	}
	if (elements[parts[i].type].Properties&PROP_INDESTRUCTIBLE && !ignore_indestructible)
		return false;
	if (elements[t].Func_Create_Allowed)
	{
		if (!(*(elements[t].Func_Create_Allowed))(this, i, x, y, t))
			return false;
	}


	int oldType = parts[i].type;
	if (oldType)
		elementCount[oldType]--;

	parts[i].type = t;
	pmap_remove(i, x, y);
	if (t)
	{
		pmap_add(i, x, y, t);
		elementCount[t]++;
	}
	if (elements[oldType].Func_ChangeType)
	{
		(*(elements[oldType].Func_ChangeType))(this, i, x, y, oldType, t);
	}
	if (elements[t].Func_ChangeType)
	{
		(*(elements[t].Func_ChangeType))(this, i, x, y, oldType, t);
	}
	return true;
}

// used by lua to change type while deleting any particle specific info, and also keep pmap / elementCount up to date
void Simulation::part_change_type_force(int i, int t)
{
	int x = (int)(parts[i].x), y = (int)(parts[i].y);
	if (t<0 || t>=PT_NUM)
		return;

	int oldType = parts[i].type;
	if (oldType)
		elementCount[oldType]--;
	parts[i].type = t;
	pmap_remove(i, x, y);
	if (t)
	{
		pmap_add(i, x, y, t);
		elementCount[t]++;
	}

	if (elements[oldType].Func_ChangeType)
	{
		(*(elements[oldType].Func_ChangeType))(this, i, x, y, oldType, t);
	}
	if (elements[t].Func_ChangeType)
	{
		(*(elements[t].Func_ChangeType))(this, i, x, y, oldType, t);
	}
}

// kills particle ID #i
void Simulation::part_kill(int i)
{
	if (i < 0 || i >= NPART)
		return;

	int x = (int)(parts[i].x + 0.5f);
	int y = (int)(parts[i].y + 0.5f);

	int t = parts[i].type;
	if (t && elements[t].Func_ChangeType)
	{
		(*(elements[t].Func_ChangeType))(this, i, x, y, t, PT_NONE);
	}

	if (x >= 0 && y >= 0 && x < XRES && y < YRES)
		pmap_remove(i, x, y);
	if (t == PT_NONE) // TODO: remove this? (//This shouldn't happen anymore, but it's here just in case)
		return;
	elementCount[t]--;
	part_free(i);
}

// calls kill_part with the particle located at (x,y)
void Simulation::part_delete(int x, int y)
{
	if (x<0 || y<0 || x>=XRES || y>=YRES)
		return;

	if (photons[y][x])
		part_kill(ID(photons[y][x]));
	else if (pmap[y][x])
		part_kill(ID(pmap[y][x]));
}

std::string Simulation::ElementResolve(int type, int ctype) const
{
	if (type == PT_LIFE)
	{
		if (ctype >= 0 && ctype < NGOL)
		{
			return builtinGol[ctype].name;
		}
		auto *cgol = static_cast<LIFE_ElementDataContainer&>(*elementData[PT_LIFE]).GetCustomGOLByRule(ctype);
		if (cgol)
		{
			return cgol->nameString;
		}
		return SerialiseGOLRule(ctype);
	}
	else if (type >= 0 && type < PT_NUM)
		return elements[type].Name;
	return "Empty";
}

char Simulation::GetEdgeMode()
{
	return saveEdgeMode == -1 ? edgeMode : saveEdgeMode;
}

void Simulation::SetEdgeMode(char edgeMode)
{
	if (edgeMode < 0 || edgeMode >= NUM_EDGEMODES)
		edgeMode = 0;

	if (edgeMode == EDGE_SOLID)
		draw_bframe();
	else
		erase_bframe();

	this->edgeMode = edgeMode;
	this->saveEdgeMode = -1;
}

void Simulation::ClearArea(int x, int y, int w, int h)
{
	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w >= XRES)
	{
		w = XRES - x;
	}
	if (y + h >= YRES)
	{
		h = YRES - y;
	}

	float fx = x - .5f, fy = y - .5f;
	for (int i = 0; i <= parts_lastActiveIndex; i++)
	{
		if (parts[i].type)
			if (parts[i].x >= fx && parts[i].x <= fx + w && parts[i].y >= fy && parts[i].y <= fy + h)
				part_kill(i);
	}
	for (int cy = 0; cy < h; cy++)
	{
		for (int cx = 0; cx < w; cx++)
		{
			bmap[(cy+y)/CELL][(cx+x)/CELL] = 0;
		}
	}
	DeleteSignsInArea(Point(x, y), Point(x+w, y+h));
}

void Simulation::GetGravityField(int x, int y, float particleGrav, float newtonGrav, float & pGravX, float & pGravY)
{
	switch (gravityMode)
	{
	default:
	case GRAV_VERTICAL: //normal, vertical gravity
		pGravX = 0;
		pGravY = particleGrav;
		break;
	case GRAV_OFF: //no gravity
		pGravX = 0;
		pGravY = 0;
		break;
	case GRAV_RADIAL: //radial gravity
	{
		pGravX = 0;
		pGravY = 0;
		auto dx = float(x - XCNTR);
		auto dy = float(y - YCNTR);
		if (dx || dy)
		{
			auto pGravD = 0.01f - hypotf(dx, dy);
			pGravX = particleGrav * (dx / pGravD);
			pGravY = particleGrav * (dy / pGravD);
		}
		break;
	}
	case GRAV_CUSTOM: //custom gravity
		pGravX = particleGrav * customGravityX;
		pGravY = particleGrav * customGravityY;
		break;
	}
	if (newtonGrav)
	{
		pGravX += newtonGrav * grav->gravx[(y/CELL)*(XRES/CELL)+(x/CELL)];
		pGravY += newtonGrav * grav->gravy[(y/CELL)*(XRES/CELL)+(x/CELL)];
	}
}

/* Recalculates the pfree/parts[].life linked list for particles with ID <= parts_lastActiveIndex.
 * This ensures that future particle allocations are done near the start of the parts array, to keep parts_lastActiveIndex low.
 * parts_lastActiveIndex is also decreased if appropriate.
 * Does not modify or even read any particles beyond parts_lastActiveIndex */
void Simulation::RecalcFreeParticles(bool doLifeDec)
{
	int x, y, t;
	int lastPartUsed = 0;
	int lastPartUnused = -1;

	std::fill_n(&pmap[0][0], XRES*YRES, 0);
	std::fill_n(&pmap_count[0][0], XRES*YRES, 0);
	std::fill_n(&photons[0][0], XRES*YRES, 0);

	NUM_PARTS = 0;
	//the particle loop that resets the pmap/photon maps every frame, to update them.
	for (int i = 0; i <= parts_lastActiveIndex; i++)
	{
		if (parts[i].type)
		{
			t = parts[i].type;
			x = (int)(parts[i].x + 0.5f);
			y = (int)(parts[i].y + 0.5f);
			bool inBounds = false;
			if (parts[i].flags & FLAG_SKIPMOVE)
				parts[i].flags &= ~FLAG_SKIPMOVE;
			if (x >= 0 && y >= 0 && x < XRES && y < YRES)
			{
#ifndef NOMOD
				if (t == PT_PINV && ID(parts[i].tmp2) >= i)
					parts[i].tmp2 = 0;
#endif
				if (elements[t].Properties & TYPE_ENERGY)
					photons[y][x] = PMAP(i, t);
				else
				{
#ifdef NOMOD
					if (!pmap[y][x] || (t != PT_INVIS && t != PT_FILT))
						pmap[y][x] = PMAP(i, t);

					if (t != PT_THDR && t != PT_EMBR && t != PT_FIGH && t != PT_PLSM)
						pmap_count[y][x]++;
#else
					// Particles are sometimes allowed to go inside INVS and FILT
					// To make particles collide correctly when inside these elements, these elements must not overwrite an existing pmap entry from particles inside them
					if (!pmap[y][x] || (t != PT_INVIS && t != PT_FILT && (t != PT_MOVS || TYP(pmap[y][x]) == PT_MOVS) && TYP(pmap[y][x]) != PT_PINV))
						pmap[y][x] = PMAP(i, t);
					else if (TYP(pmap[y][x]) == PT_PINV)
						parts[ID(pmap[y][x])].tmp2 = PMAP(i, t);

					// Count number of particles at each location, for excess stacking check
					// (does not include energy particles or THDR - currently no limit on stacking those)
					if (t != PT_THDR && t != PT_EMBR && t != PT_FIGH && t != PT_PLSM && t != PT_MOVS)
						pmap_count[y][x]++;
#endif
				}
				inBounds = true;
			}
			lastPartUsed = i;
			NUM_PARTS++;
			//decrease the life of certain elements by 1 every frame
			if (doLifeDec && (!sys_pause || framerender))
			{
				// TODO: is this check needed? This shouldn't be possible
				if (t < 0 || t >= PT_NUM)
				{
					part_kill(i);
				}
				// If this is in non-activated stasis wall, don't update life
				if (!inBounds || (!(bmap[y/CELL][x/CELL] == WL_STASIS && emap[y/CELL][x/CELL]<8)))
				{
					unsigned int elem_properties = elements[t].Properties;
					if (parts[i].life > 0 && (elem_properties & PROP_LIFE_DEC))
					{
						// automatically decrease life
						parts[i].life--;
						if (parts[i].life <= 0 && (elem_properties & (PROP_LIFE_KILL_DEC | PROP_LIFE_KILL)))
						{
							// kill on change to no life
							part_kill(i);
						}
					}
					else if (parts[i].life <= 0 && (elem_properties & PROP_LIFE_KILL)
					        && !(bmap[y/CELL][x/CELL] == WL_STASIS && emap[y/CELL][x/CELL]<8))
					{
						// kill if no life
						part_kill(i);
					}
				}
			}
		}
		else
		{
			if (lastPartUnused < 0)
				pfree = i;
			else
				parts[lastPartUnused].life = i;
			lastPartUnused = i;
		}
	}

	if (lastPartUnused == -1)
	{
		pfree = (parts_lastActiveIndex >= NPART - 1) ? -1 : parts_lastActiveIndex + 1;
	}
	else
	{
		parts[lastPartUnused].life = (parts_lastActiveIndex >= NPART - 1) ? -1 : parts_lastActiveIndex + 1;
	}
	parts_lastActiveIndex = lastPartUsed;
}

void Simulation::UpdateBefore()
{
#ifdef LUACONSOLE
	auto ev = BeforeSimEvent();
	HandleEvent(LuaEvents::beforesim, &ev);
#endif

	//update wallmaps
	for (int y = 0; y < YRES/CELL; y++)
	{
		for (int x = 0; x < XRES/CELL; x++)
		{
			if (emap[y][x])
				emap[y][x]--;
			air->blockair[y][x] = (bmap[y][x]==WL_WALL || bmap[y][x]==WL_WALLELEC || bmap[y][x]==WL_BLOCKAIR || (bmap[y][x]==WL_EWALL && !emap[y][x]));
			air->blockairh[y][x] = (air->blockair[y][x] || bmap[y][x]==WL_GRAV) ? 0x8 : 0;
		}
	}

	//check for excessive stacked particles, create BHOL if found
	if (forceStackingCheck || RNG::Ref().chance(1, 10))
	{
		bool excessiveStackingFound = false;
		forceStackingCheck = 0;
		for (int y = 0; y < YRES; y++)
		{
			for (int x = 0; x < XRES; x++)
			{
				//Use a threshold, since some particle stacking can be normal (e.g. BIZR + FILT)
				//Setting pmap_count[y][x] > NPART means BHOL will form in that spot
				if (pmap_count[y][x] > 5)
				{
					if (bmap[y/CELL][x/CELL] == WL_EHOLE)
					{
						//Allow more stacking in E-hole
						if (pmap_count[y][x] > 1500)
						{
							pmap_count[y][x] = pmap_count[y][x] + NPART;
							excessiveStackingFound = true;
						}
					}
					//Random chance to turn into BHOL that increases with the amount of stacking, up to a threshold where it is certain to turn into BHOL
					else if (pmap_count[y][x] > 1500 || RNG::Ref().between(0, 1599) <= pmap_count[y][x]+100)
					{
						pmap_count[y][x] = pmap_count[y][x] + NPART;
						excessiveStackingFound = true;
					}
				}
			}
		}
		if (excessiveStackingFound)
		{
			for (int i = 0; i <= parts_lastActiveIndex; i++)
			{
				if (parts[i].type)
				{
					int t = parts[i].type;
					int x = (int)(parts[i].x+0.5f);
					int y = (int)(parts[i].y+0.5f);
					if (x >= 0 && y >= 0 && x < XRES && y < YRES && !(elements[t].Properties&TYPE_ENERGY))
					{
						if (pmap_count[y][x] >= NPART)
						{
							if (pmap_count[y][x] > NPART)
							{
								part_create(i, x, y, PT_NBHL);
								parts[i].temp = MAX_TEMP;
								parts[i].tmp = pmap_count[y][x] - NPART;//strength of grav field
								if (parts[i].tmp > 51200)
									parts[i].tmp = 51200;
								pmap_count[y][x] = NPART;
							}
							else
							{
								part_kill(i);
							}
						}
					}
				}
			}
		}
	}

	// For elements with extra data, run special update functions
	// This does things like LIFE recalculation and LOLZ patterns
	for (int t = 1; t < PT_NUM; t++)
	{
		if (elementData[t])
		{
			elementData[t]->Simulation_BeforeUpdate(this);
		}
	}

	// lightning recreation time (TODO: move to elementData)
	if (lightningRecreate)
		lightningRecreate--;
}

void Simulation::UpdateParticles(int start, int end)
{
	// The main particle loop function, goes over all particles.
	for (int i = start; i <= end && i <= parts_lastActiveIndex; i++)
		if (parts[i].type)
		{
			debug_mostRecentlyUpdated = i;

			UpdateParticle(i);
		}
}

void Simulation::UpdateAfter()
{
	debug_mostRecentlyUpdated = -1;

	// For elements with extra data, run special update functions
	// Used only for moving solids
	for (int t = 1; t < PT_NUM; t++)
	{
		if (elementData[t])
		{
			elementData[t]->Simulation_AfterUpdate(this);
		}
	}

#ifdef LUACONSOLE
	auto ev = AfterSimEvent();
	HandleEvent(LuaEvents::aftersim, &ev);
#endif

	// Only update air if not paused
	if (!sys_pause||framerender)
	{
		globalSim->air->UpdateAir();
		globalSim->air->UpdateAirHeat(globalSim);
	}

	if (globalSim->grav->gravWallChanged)
	{
		globalSim->grav->CalculateMask();
		globalSim->grav->gravWallChanged = false;
	}

	if (!sys_pause||framerender)
	{
		globalSim->grav->UpdateAsync(); //Check for updated velocity maps from gravity thread
		memset(globalSim->grav->gravmap, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float)); //Clear the old gravmap
	}

	if (framerender)
		framerender--;
}

void Simulation::Tick()
{
	if (debug_currentParticle == 0)
		RecalcFreeParticles(true);
	if (!sys_pause || framerender)
	{
		UpdateBefore();
		UpdateParticles(0, NPART);
		UpdateAfter();
		currentTick++;
	}
	// In automatic heat mode, calculate highest and lowest temperature points (maybe could be moved)
	if (heatmode == 1)
	{
		highesttemp = MIN_TEMP;
		lowesttemp = MAX_TEMP;
		for (int i = 0; i <= parts_lastActiveIndex; i++)
		{
			if (parts[i].type)
			{
				if (parts[i].temp > highesttemp)
					highesttemp = (int)parts[i].temp;
				if (parts[i].temp < lowesttemp)
					lowesttemp = (int)parts[i].temp;
			}
		}
	}
}

std::string Simulation::ParticleDebug(int mode, int x, int y)
{
	int debug_currentParticle = globalSim->debug_currentParticle;
	int i;
	std::stringstream logmessage;

	// update one particle at a time
	if (mode == 0)
	{
		if (!NUM_PARTS)
			return "";
		i = debug_currentParticle;
		while (i < NPART - 1 && !globalSim->parts[i].type)
			i++;
		if (i == NPART - 1)
			logmessage << "End of particles reached, updated sim";
		else
			logmessage << "Updated particle #" << i;
	}
	// update all particles up to particle under mouse (or to end of sim)
	else if (mode == 1)
	{
		if (x < 0 || x >= XRES || y < 0 || y >= YRES || !pmap[y][x] || (i = ID(pmap[y][x])) < debug_currentParticle)
		{
			i = NPART - 1;
			logmessage << "Updated particles from #" << debug_currentParticle << " to end, updated sim";
		}
		else
			logmessage << "Updated particles #" << debug_currentParticle << " through #" << i;
	}

	// call simulation functions run before updating particles if we are updating #0
	if (debug_currentParticle == 0)
	{
		framerender = 1;
		globalSim->RecalcFreeParticles(true);
		globalSim->UpdateBefore();
		framerender = 0;
	}
	// update the particles
	globalSim->UpdateParticles(debug_currentParticle, i);
	if (i < NPART-1)
		globalSim->debug_currentParticle = i+1;
	// we reached the end, call simulation functions run after updating particles
	else
	{
		globalSim->UpdateAfter();
		globalSim->currentTick++;
		globalSim->debug_currentParticle = 0;
	}
	return logmessage.str();
}

int PCLN_update(UPDATE_FUNC_ARGS);
int CLNE_update(UPDATE_FUNC_ARGS);
int PBCN_update(UPDATE_FUNC_ARGS);
int BCLN_update(UPDATE_FUNC_ARGS);
int MOVS_update(UPDATE_FUNC_ARGS);

bool Simulation::UpdateParticle(int i)
{
	unsigned int t = (unsigned int)parts[i].type;
	int x = (int)(parts[i].x+0.5f);
	int y = (int)(parts[i].y+0.5f);
	bool transitionOccurred = false;

	// Kill a particle off screen
	if (x < CELL || y < CELL || x >= XRES-CELL || y >= YRES-CELL)
	{
		part_kill(i);
		return true;
	}

	// Kill a particle in a wall where it isn't supposed to go
	if (bmap[y/CELL][x/CELL] &&
		(bmap[y/CELL][x/CELL] == WL_WALL ||
			bmap[y/CELL][x/CELL] == WL_WALLELEC ||
			bmap[y/CELL][x/CELL] == WL_ALLOWAIR ||
			bmap[y/CELL][x/CELL] == WL_DESTROYALL ||
			(bmap[y/CELL][x/CELL] == WL_ALLOWLIQUID && !(elements[t].Properties&TYPE_LIQUID)) ||
			(bmap[y/CELL][x/CELL] == WL_ALLOWPOWDER && !(elements[t].Properties&TYPE_PART)) ||
			(bmap[y/CELL][x/CELL] == WL_ALLOWGAS && !(elements[t].Properties&TYPE_GAS)) || //&&  elements[t].Falldown!=0 && t!=PT_FIRE && t!=PT_SMKE && t!=PT_HFLM) ||
			(bmap[y/CELL][x/CELL] == WL_ALLOWENERGY && !(elements[t].Properties&TYPE_ENERGY)) ||
			(bmap[y/CELL][x/CELL] == WL_EWALL && !emap[y/CELL][x/CELL])
#ifdef NOMOD
		  ) && t!=PT_STKM && t!=PT_STKM2 && t!=PT_FIGH)
#else
		  ) && t!=PT_STKM && t!=PT_STKM2 && t!=PT_FIGH && t != PT_MOVS)
#endif
	{
		part_kill(i);
		return true;
	}

	// Make sure that STASIS'd particles don't tick.
	if (bmap[y/CELL][x/CELL] == WL_STASIS && emap[y/CELL][x/CELL] < 8)
		return false;

	if (bmap[y/CELL][x/CELL]==WL_DETECT && emap[y/CELL][x/CELL]<8)
		set_emap(x/CELL, y/CELL);

	if (parts[i].flags&FLAG_SKIPMOVE)
		return false;

	//adding to velocity from the particle's velocity
	air->vx[y/CELL][x/CELL] = air->vx[y/CELL][x/CELL]*elements[t].AirLoss + elements[t].AirDrag*parts[i].vx;
	air->vy[y/CELL][x/CELL] = air->vy[y/CELL][x/CELL]*elements[t].AirLoss + elements[t].AirDrag*parts[i].vy;

	if (elements[t].HotAir)
	{
		if (t == PT_GAS || t == PT_NBLE)
		{
			if (air->pv[y/CELL][x/CELL] < 3.5f)
				air->pv[y/CELL][x/CELL] += elements[t].HotAir*(3.5f - air->pv[y/CELL][x/CELL]);
			if (y+CELL < YRES && air->pv[y/CELL+1][x/CELL] < 3.5f)
				air->pv[y/CELL+1][x/CELL] += elements[t].HotAir*(3.5f - air->pv[y/CELL+1][x/CELL]);
			if (x+CELL < XRES)
			{
				if (air->pv[y/CELL][x/CELL+1] < 3.5f)
					air->pv[y/CELL][x/CELL+1] += elements[t].HotAir*(3.5f - air->pv[y/CELL][x/CELL+1]);
				if (y+CELL<YRES && air->pv[y/CELL+1][x/CELL+1] < 3.5f)
					air->pv[y/CELL+1][x/CELL+1] += elements[t].HotAir*(3.5f - air->pv[y/CELL+1][x/CELL+1]);
			}
		}
		//add the hotair variable to the pressure map, like black hole, or white hole.
		else
		{
			float value = elements[t].HotAir;
			value = restrict_flt(value, -256.0f, 256.0f);
			air->pv[y/CELL][x/CELL] += value;
			if (y+CELL < YRES)
				air->pv[y/CELL+1][x/CELL] += value;
			if (x+CELL < XRES)
			{
				air->pv[y/CELL][x/CELL+1] += value;
				if (y+CELL < YRES)
					air->pv[y/CELL+1][x/CELL+1] += value;
			}
		}
	}
	float pGravX = 0, pGravY = 0;
	if (!(elements[t].Properties & TYPE_SOLID) && (elements[t].Gravity || elements[t].NewtonianGravity))
	{
		GetGravityField(x, y, elements[t].Gravity, elements[t].NewtonianGravity, pGravX, pGravY);
	}

	//velocity updates for the particle
	parts[i].vx *= elements[t].Loss;
	parts[i].vy *= elements[t].Loss;
	//particle gets velocity from the vx and vy maps
	parts[i].vx += elements[t].Advection*air->vx[y/CELL][x/CELL] + pGravX;
	parts[i].vy += elements[t].Advection*air->vy[y/CELL][x/CELL] + pGravY;


	if (elements[t].Diffusion)//the random diffusion that gases have
	{
		parts[i].vx += elements[t].Diffusion * (2.0f * RNG::Ref().uniform01() - 1);
		parts[i].vy += elements[t].Diffusion * (2.0f * RNG::Ref().uniform01() - 1);
	}

	//surround_space stores the number of empty spaces around a particle, nt stores the number of empty spaces + the number of particles of a different type
	int surround_space = 0, surround_particle = 0, nt = 0;
	//surround stores the 8 particle types around the current one, used in heat transfer
	int surround[8];
	for (int nx = -1; nx <= 1; nx++)
		for (int ny = -1; ny <= 1; ny++)
		{
			if (nx || ny)
			{
				int r;
				surround[surround_particle] = r = pmap[y+ny][x+nx];
				surround_particle++;

				surround_space += (!TYP(r)); // count empty space
				nt += (TYP(r) != t); // count empty space and particles of different type
			}
		}

	if (!legacy_enable)
	{
		if (TransferHeat(i, t, surround))
		{
			transitionOccurred = true;
			t = parts[i].type;
		}
		if (!t)
			return true;
	}

	//spark updates from walls
	if ((elements[t].Properties&PROP_CONDUCTS) || t == PT_SPRK)
	{
		int nx = x%CELL;
		if (nx == 0)
			nx = x/CELL - 1;
		else if (nx == CELL-1)
			nx = x/CELL + 1;
		else
			nx = x/CELL;

		int ny = y%CELL;
		if (ny == 0)
			ny = y/CELL - 1;
		else if (ny == CELL-1)
			ny = y/CELL + 1;
		else
			ny = y/CELL;

		if (nx >= 0 && ny >= 0 && nx < XRES/CELL && ny < YRES/CELL)
		{
			if (t != PT_SPRK)
			{
				if (emap[ny][nx] == 12 && !parts[i].life && bmap[ny][nx] != WL_STASIS)
				{
					spark_conductive(i, x, y);
					parts[i].life = 4;
					t = PT_SPRK;
				}
			}
			else if (bmap[ny][nx] == WL_DETECT || bmap[ny][nx] == WL_EWALL || bmap[ny][nx] == WL_ALLOWLIQUID || bmap[ny][nx] == WL_WALLELEC || bmap[ny][nx] == WL_ALLOWALLELEC || bmap[ny][nx] == WL_EHOLE)
				set_emap(nx, ny);
		}
	}

	//the basic explosion, from the .explosive variable
	if (!(elements[t].Properties&PROP_INDESTRUCTIBLE) && (elements[t].Explosive&2) && air->pv[y/CELL][x/CELL] > 2.5f)
	{
		parts[i].life = RNG::Ref().between(180, 259);
		parts[i].temp = restrict_flt(elements[PT_FIRE].DefaultProperties.temp + (elements[t].Flammable/2), MIN_TEMP, MAX_TEMP);
		t = PT_FIRE;
		part_change_type(i, x, y, t);
		air->pv[y/CELL][x/CELL] += 0.25f * CFDS;
	}

	if (!(elements[t].Properties&PROP_INDESTRUCTIBLE) && (elements[t].HighPressureTransitionThreshold != -1 || elements[t].HighPressureTransitionElement != -1))
	{
		if (CheckPressureTransitions(i, t))
		{
			transitionOccurred = true;
			t = parts[i].type;
		}
		if (!t)
			return true;
	}

#ifdef LUACONSOLE
	if (lua_el_mode[t] != UPDATE_REPLACE)
	{
#endif
		if (elements[t].Properties&PROP_POWERED)
		{
			if (update_POWERED(this, i, x, y, surround_space, nt))
				return true;
		}
		if (elements[t].Properties&PROP_CLONE)
		{
			if (elements[t].Properties&PROP_POWERED)
				PCLN_update(this, i, x, y, surround_space, nt);
			else
				CLNE_update(this, i, x, y, surround_space, nt);
		}
		else if (elements[t].Properties&PROP_BREAKABLECLONE)
		{
			if (elements[t].Properties&PROP_POWERED)
			{
				if (PBCN_update(this, i, x, y, surround_space, nt))
					return true;
			}
			else
			{
				if (BCLN_update(this, i, x, y, surround_space, nt))
					return true;
			}
		}
#ifdef LUACONSOLE
	}
#endif

	//call the particle update function, if there is one
	if (elements[t].Update)
	{
		if ((*(elements[t].Update))(this, i, x, y, surround_space, nt))
			return true;
		x = (int)(parts[i].x+0.5f);
		y = (int)(parts[i].y+0.5f);
	}

	if (legacy_enable)//if heat sim is off
		update_legacy_all(this, i, x, y,surround_space, nt);

	//if its dead, skip to next particle
	if (parts[i].type == PT_NONE)
		return true;

#ifndef NOMOD
	if (parts[i].flags&FLAG_EXPLODE)
	{
		if (RNG::Ref().chance(1, 10))
		{
			parts[i].flags &= ~FLAG_EXPLODE;
			air->pv[y/CELL][x/CELL] += 5.0f;
			if (RNG::Ref().chance(1, 3))
			{
				if (RNG::Ref().chance(1, 2))
				{
					part_create(i, x, y, PT_BOMB);
					parts[i].temp = MAX_TEMP;
				}
				else
				{
					part_create(i, x, y, PT_PLSM);
					parts[i].temp = MAX_TEMP;
				}
			}
			else
			{
				part_create(i, x, y, PT_EMBR);
				parts[i].temp = MAX_TEMP;
				parts[i].vx = RNG::Ref().between(-10, 10);
				parts[i].vy = RNG::Ref().between(-10, 10);
			}
			return true;
		}
	}
#endif

	if (transitionOccurred)
		return false;

	if (!parts[i].vx && !parts[i].vy)//if its not moving, skip to next particle, movement code is next
		return false;

	float mv = std::max(fabsf(parts[i].vx), fabsf(parts[i].vy));
	int fin_x, fin_y, clear_x, clear_y;
	float fin_xf, fin_yf, clear_xf, clear_yf;
	if (mv < ISTP || std::isnan(mv))
	{
		clear_x = x;
		clear_y = y;
		clear_xf = parts[i].x;
		clear_yf = parts[i].y;
		fin_xf = clear_xf + parts[i].vx;
		fin_yf = clear_yf + parts[i].vy;
		fin_x = (int)(fin_xf+0.5f);
		fin_y = (int)(fin_yf+0.5f);
	}
	else
	{
		if (mv > MAX_VELOCITY)
		{
			parts[i].vx *= MAX_VELOCITY/mv;
			parts[i].vy *= MAX_VELOCITY/mv;
			mv = MAX_VELOCITY;
		}
		// interpolate to see if there is anything in the way
		float dx = parts[i].vx*ISTP/mv;
		float dy = parts[i].vy*ISTP/mv;
		fin_xf = parts[i].x;
		fin_yf = parts[i].y;
		fin_x = (int)(fin_xf+0.5f);
		fin_y = (int)(fin_yf+0.5f);
		bool closedEholeStart = this->InBounds(fin_x, fin_y) && (bmap[fin_y/CELL][fin_x/CELL] == WL_EHOLE && !emap[fin_y/CELL][fin_x/CELL]);
		while (1)
		{
			mv -= ISTP;
			fin_xf += dx;
			fin_yf += dy;
			fin_x = (int)(fin_xf+0.5f);
			fin_y = (int)(fin_yf+0.5f);
			if (GetEdgeMode() == EDGE_LOOP)
			{
				bool x_ok = (fin_xf >= CELL-.5f && fin_xf < XRES-CELL-.5f);
				bool y_ok = (fin_yf >= CELL-.5f && fin_yf < YRES-CELL-.5f);
				if (!x_ok)
					fin_xf = remainder_p(fin_xf-CELL+.5f, XRES-CELL*2.0f)+CELL-.5f;
				if (!y_ok)
					fin_yf = remainder_p(fin_yf-CELL+.5f, YRES-CELL*2.0f)+CELL-.5f;
				fin_x = (int)(fin_xf+0.5f);
				fin_y = (int)(fin_yf+0.5f);
			}
			if (mv <= 0.0f)
			{
				// nothing found
				fin_xf = parts[i].x + parts[i].vx;
				fin_yf = parts[i].y + parts[i].vy;
				if (GetEdgeMode() == EDGE_LOOP)
				{
					bool x_ok = (fin_xf >= CELL-.5f && fin_xf < XRES-CELL-.5f);
					bool y_ok = (fin_yf >= CELL-.5f && fin_yf < YRES-CELL-.5f);
					if (!x_ok)
						fin_xf = remainder_p(fin_xf-CELL+.5f, XRES-CELL*2.0f)+CELL-.5f;
					if (!y_ok)
						fin_yf = remainder_p(fin_yf-CELL+.5f, YRES-CELL*2.0f)+CELL-.5f;
				}
				fin_x = (int)(fin_xf+0.5f);
				fin_y = (int)(fin_yf+0.5f);
				clear_xf = fin_xf-dx;
				clear_yf = fin_yf-dy;
				clear_x = (int)(clear_xf+0.5f);
				clear_y = (int)(clear_yf+0.5f);
				break;
			}
			//block if particle can't move (0), or some special cases where it returns 1 (can_move = 3 but returns 1 meaning particle will be eaten)
			//also photons are still blocked (slowed down) by any particle (even ones it can move through), and absorb wall also blocks particles
			int eval = EvalMove(t, fin_x, fin_y);
			if (!eval || (can_move[t][TYP(pmap[fin_y][fin_x])] == 3 && eval == 1) || (t == PT_PHOT && pmap[fin_y][fin_x]) || bmap[fin_y/CELL][fin_x/CELL]==WL_DESTROYALL || closedEholeStart!=(bmap[fin_y/CELL][fin_x/CELL] == WL_EHOLE && !emap[fin_y/CELL][fin_x/CELL]))
			{
				// found an obstacle
				clear_xf = fin_xf-dx;
				clear_yf = fin_yf-dy;
				clear_x = (int)(clear_xf+0.5f);
				clear_y = (int)(clear_yf+0.5f);
				break;
			}
			if (bmap[fin_y/CELL][fin_x/CELL] == WL_DETECT && emap[fin_y/CELL][fin_x/CELL] < 8)
				set_emap(fin_x/CELL, fin_y/CELL);
		}
	}

	bool stagnant = parts[i].flags & FLAG_STAGNANT;
	parts[i].flags &= ~FLAG_STAGNANT;

	if (t == PT_STKM || t == PT_STKM2 || t == PT_FIGH)
	{
		//head movement, let head pass through anything
		int startingx = (int)((float)parts[i].x+0.5f);
		int startingy = (int)((float)parts[i].y+0.5f);
		parts[i].x += parts[i].vx;
		parts[i].y += parts[i].vy;
		int nx = (int)((float)parts[i].x+0.5f);
		int ny = (int)((float)parts[i].y+0.5f);
		if (edgeMode == EDGE_LOOP)
		{
			bool x_ok = (nx >= CELL && nx < XRES-CELL);
			bool y_ok = (ny >= CELL && ny < YRES-CELL);
			int oldnx = nx, oldny = ny;
			if (!x_ok)
			{
				parts[i].x = remainder_p(parts[i].x-CELL+.5f, XRES-CELL*2.0f)+CELL-.5f;
				nx = (int)((float)parts[i].x+0.5f);
			}
			if (!y_ok)
			{
				parts[i].y = remainder_p(parts[i].y-CELL+.5f, YRES-CELL*2.0f)+CELL-.5f;
				ny = (int)((float)parts[i].y+0.5f);
			}
			
			if (!x_ok || !y_ok) //when moving from left to right stickmen might be able to fall through solid things, fix with "eval_move(t, nx+diffx, ny+diffy, NULL)" but then they die instead
			{
				//adjust stickmen legs
				Stickman *stickman = NULL;
				if (t == PT_STKM)
					stickman = static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman1();
				else if (t == PT_STKM2)
					stickman = static_cast<STKM_ElementDataContainer&>(*elementData[PT_STKM]).GetStickman2();
				else if (t == PT_FIGH && parts[i].tmp >= 0 && parts[i].tmp < static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).MaxFighters())
					stickman = static_cast<FIGH_ElementDataContainer&>(*elementData[PT_FIGH]).Get((unsigned char)parts[i].tmp);
	
				if (stickman)
				{
					for (int j = 0; j < 16; j += 2)
					{
						stickman->legs[j] += (nx-oldnx);
						stickman->legs[j+1] += (ny-oldny);
						stickman->accs[j/2] *= .95f;
					}
				}
				parts[i].vy *= .95f;
				parts[i].vx *= .95f;
			}
		}
		if (ny!=y || nx!=x)
		{
			Move(i, startingx, startingy, parts[i].x, parts[i].y);
		}
	}
	else if (elements[t].Properties & TYPE_ENERGY)
	{
		if (t == PT_PHOT)
		{
			if (EvalMove(PT_PHOT, fin_x, fin_y))
			{
				int rt = TYP(pmap[fin_y][fin_x]);
				int lt = TYP(pmap[y][x]);

				bool rt_glas = (rt == PT_GLAS) || (rt == PT_BGLA);
				bool lt_glas = (lt == PT_GLAS) || (lt == PT_BGLA);
				if ((rt_glas && !lt_glas) || (lt_glas && !rt_glas))
				{
					float nrx, nry;
					if (!GetNormalInterp(REFRACT|t, parts[i].x, parts[i].y, parts[i].vx, parts[i].vy, &nrx, &nry))
					{
						part_kill(i);
						return true;
					}

					int r = get_wavelength_bin(&parts[i].ctype);
					if (r == -1 || !(parts[i].ctype&0x3FFFFFFF))
					{
						part_kill(i);
						return true;
					}
					float nn = GLASS_IOR - GLASS_DISP*(r-30)/30.0f;
					nn *= nn;
					bool enter = rt_glas && !lt_glas;
					nrx = enter ? -nrx : nrx;
					nry = enter ? -nry : nry;
					nn = enter ? 1.0f/nn : nn;
					float ct1 = parts[i].vx*nrx + parts[i].vy*nry;
					float ct2 = 1.0f - (nn*nn)*(1.0f-(ct1*ct1));
					if (ct2 < 0.0f)
					{
						//total internal reflection
						parts[i].vx -= 2.0f*ct1*nrx;
						parts[i].vy -= 2.0f*ct1*nry;
						fin_xf = parts[i].x;
						fin_yf = parts[i].y;
						fin_x = x;
						fin_y = y;
					}
					else
					{
						// refraction
						ct2 = sqrtf(ct2);
						ct2 = ct2 - nn*ct1;
						parts[i].vx = nn*parts[i].vx + ct2*nrx;
						parts[i].vy = nn*parts[i].vy + ct2*nry;
					}
				}
			}
		}
		//FLAG_STAGNANT set, was reflected on previous frame
		if (stagnant)
		{
			// cast coords as int then back to float for compatibility with existing saves
			if (!DoMove(i, x, y, (float)fin_x, (float)fin_y) && parts[i].type)
			{
				part_kill(i);
				return true;
			}
		}
		else if (!DoMove(i, x, y, fin_xf, fin_yf))
		{
			float nrx, nry;
			if (parts[i].type == PT_NONE)
				return true;
			//reflection
			parts[i].flags |= FLAG_STAGNANT;
			if (t == PT_NEUT && RNG::Ref().chance(1, 10))
			{
				part_kill(i);
				return true;
			}
			int r = pmap[fin_y][fin_x];

			if ((TYP(r)==PT_PIPE || TYP(r) == PT_PPIP) && !TYP(parts[ID(r)].ctype))
			{
				PIPE_transfer_part_to_pipe(this, parts+i, parts+(ID(r)));
				return true;
			}

			if (t == PT_PHOT)
			{
				auto mask = elements[TYP(r)].PhotonReflectWavelengths;
				if (TYP(r) == PT_LITH)
				{
					int wl_bin = parts[ID(r)].ctype / 4;
					if (wl_bin < 0) wl_bin = 0;
					if (wl_bin > 25) wl_bin = 25;
					mask = (0x1F << wl_bin);
				}
				else if (TYP(r) == PT_SEED)
				{
					// Reflect different wavelengths based on SEED's color genes
					int colour = (parts[ID(r)].ctype >> PLNT_COLOUR) & 0x3f;

					mask |= ((colour & 0b110000) != 0) ? 0 : (1 << 25); // Red
					mask |= ((colour & 0b001100) != 0) ? 0 : (1 << 15); // Green
					mask |= ((colour & 0b000011) != 0) ? 0 : (1 << 5); // Blue
				}
				parts[i].ctype &= mask;
			}

			if (GetNormalInterp(t, parts[i].x, parts[i].y, parts[i].vx, parts[i].vy, &nrx, &nry))
			{
				if (TYP(r) == PT_CRMC)
				{
					float r = RNG::Ref().between(-50, 50) * 0.01f, rx, ry, anrx, anry;
					r = r * r * r;
					rx = cosf(r); ry = sinf(r);
					anrx = rx * nrx + ry * nry;
					anry = rx * nry - ry * nrx;
					float dp = anrx*parts[i].vx + anry*parts[i].vy;
					parts[i].vx -= 2.0f*dp*anrx;
					parts[i].vy -= 2.0f*dp*anry;
				}
				else
				{
					float dp = nrx*parts[i].vx + nry*parts[i].vy;
					parts[i].vx -= 2.0f*dp*nrx;
					parts[i].vy -= 2.0f*dp*nry;
				}
				// leave the actual movement until next frame so that reflection of fast particles and refraction happen correctly
			}
			else
			{
				if (t != PT_NEUT)
				{
					part_kill(i);
					return true;
				}
				return false;
			}
			if (!(parts[i].ctype&0x3FFFFFFF) && t == PT_PHOT)
			{
				part_kill(i);
				return true;
			}
		}
	}
	// gases and solids (but not powders)
	else if (elements[t].Falldown == 0)
	{
		if (!DoMove(i, x, y, fin_xf, fin_yf))
		{
			if (parts[i].type == PT_NONE)
				return true;
			// can't move there, so bounce off
			if (fin_x > x+ISTP) fin_x = x+ISTP;
			if (fin_x < x-ISTP) fin_x = x-ISTP;
			if (fin_y > y+ISTP) fin_y = y+ISTP;
			if (fin_y < y-ISTP) fin_y = y-ISTP;
			if (DoMove(i, x, y, (float)(2 * x - fin_x), fin_y))
			{
				parts[i].vx *= elements[t].Collision;
			}
			else if (DoMove(i, x, y, fin_x, (float)(2 * y - fin_y)))
			{
				parts[i].vy *= elements[t].Collision;
			}
			else
			{
				parts[i].vx *= elements[t].Collision;
				parts[i].vy *= elements[t].Collision;
			}
		}
	}
	//liquids and powders
	else
	{
		//checking stagnant is cool, but then it doesn't update when you change it later.
		if (water_equal_test && elements[t].Falldown == 2 && RNG::Ref().chance(1, 400))
		{
			if (flood_water(x, y, i))
				return false;
		}
		if (!DoMove(i, x, y, fin_xf, fin_yf))
		{
			if (parts[i].type == PT_NONE)
				return true;
			if (fin_x != x && DoMove(i, x, y, fin_xf, clear_yf))
			{
				parts[i].vx *= elements[t].Collision;
				parts[i].vy *= elements[t].Collision;
			}
			else if (fin_y != y && DoMove(i, x, y, clear_xf, fin_yf))
			{
				parts[i].vx *= elements[t].Collision;
				parts[i].vy *= elements[t].Collision;
			}
			else
			{
				int nx, ny, s = 1;
				int r = RNG::Ref().between(0, 1) * 2 - 1; // position search direction (left/right first)
				if ((clear_x!=x || clear_y!=y || nt || surround_space) &&
					(fabsf(parts[i].vx)>0.01f || fabsf(parts[i].vy)>0.01f))
				{
					// allow diagonal movement if target position is blocked
					// but no point trying this if particle is stuck in a block of identical particles
					float dx = parts[i].vx - parts[i].vy*r;
					float dy = parts[i].vy + parts[i].vx*r;
					mv = tpt::max(fabsf(dx), fabsf(dy));
					dx /= mv;
					dy /= mv;
					if (DoMove(i, x, y, clear_xf+dx, clear_yf+dy))
					{
						parts[i].vx *= elements[t].Collision;
						parts[i].vy *= elements[t].Collision;
						return false;
					}
					float swappage = dx;
					dx = dy*r;
					dy = -swappage*r;
					if (DoMove(i, x, y, clear_xf+dx, clear_yf+dy))
					{
						parts[i].vx *= elements[t].Collision;
						parts[i].vy *= elements[t].Collision;
						return false;
					}
				}
				if (elements[t].Falldown>1 && !grav->IsEnabled() && gravityMode==GRAV_VERTICAL && parts[i].vy>fabsf(parts[i].vx))
				{
					int rt;
					s = 0;
					// stagnant is true if FLAG_STAGNANT was set for this particle in previous frame
					if (!stagnant || nt) //nt is if there is an something else besides the current particle type, around the particle
						rt = 30;//slight less water lag, although it changes how it moves a lot
					else
						rt = 10;

					if (t == PT_GEL)
						rt = (int)(parts[i].tmp*0.20f+5.0f);

					for (int j=clear_x+r; j>=0 && j>=clear_x-rt && j<clear_x+rt && j<XRES; j+=r)
					{
						if ((TYP(pmap[fin_y][j]) != t || bmap[fin_y/CELL][j/CELL])
							&& (s=DoMove(i, x, y, (float)j, fin_yf)))
						{
							nx = (int)(parts[i].x+0.5f);
							ny = (int)(parts[i].y+0.5f);
							break;
						}
						if (fin_y!=clear_y && (TYP(pmap[clear_y][j]) != t || bmap[clear_y/CELL][j/CELL])
							&& (s=DoMove(i, x, y, (float)j, clear_yf)))
						{
							nx = (int)(parts[i].x+0.5f);
							ny = (int)(parts[i].y+0.5f);
							break;
						}
						if (TYP(pmap[clear_y][j]) != t || (bmap[clear_y/CELL][j/CELL] && bmap[clear_y/CELL][j/CELL] != WL_STREAM))
							break;
					}
					r = (parts[i].vy>0) ? 1 : -1;
					if (s == 1)
						for (int j=ny+r; j>=0 && j<YRES && j>=ny-rt && j<ny+rt; j+=r)
						{
							if ((TYP(pmap[j][nx]) != t || bmap[j/CELL][nx/CELL]) && DoMove(i, nx, ny, (float)nx, (float)j))
								break;
							if (TYP(pmap[j][nx]) != t || (bmap[j/CELL][nx/CELL] && bmap[j/CELL][nx/CELL] != WL_STREAM))
								break;
						}
					else if (s==-1) {} // particle is out of bounds
					else if ((clear_x!=x||clear_y!=y) && DoMove(i, x, y, clear_xf, clear_yf)) {}
					else parts[i].flags |= FLAG_STAGNANT;
					parts[i].vx *= elements[t].Collision;
					parts[i].vy *= elements[t].Collision;
				}
				else if (elements[t].Falldown>1 && fabsf(pGravX*parts[i].vx+pGravY*parts[i].vy)>fabsf(pGravY*parts[i].vx-pGravX*parts[i].vy))
				{
					float nxf, nyf, prev_pGravX, prev_pGravY, ptGrav = elements[t].Gravity;
					int rt;
					s = 0;
					// stagnant is true if FLAG_STAGNANT was set for this particle in previous frame
					// nt is if there is something else besides the current particle type around the particle
					// 30 gives slightly less water lag, although it changes how it moves a lot
					rt = (!stagnant || nt) ? 30 : 10;
					// clear_xf, clear_yf is the last known position that the particle should almost certainly be able to move to
					nxf = clear_xf;
					nyf = clear_yf;
					nx = clear_x;
					ny = clear_y;
					// Look for spaces to move horizontally (perpendicular to gravity direction), keep going until a space is found or the number of positions examined = rt
					for (int j = 0; j < rt; j++)
					{
						// Calculate overall gravity direction
						GetGravityField(nx, ny, ptGrav, 1.0f, pGravX, pGravY);
						// Scale gravity vector so that the largest component is 1 pixel
						mv = tpt::max(fabsf(pGravX), fabsf(pGravY));
						if (mv<0.0001f) break;
						pGravX /= mv;
						pGravY /= mv;
						// Move 1 pixel perpendicularly to gravity
						// r is +1/-1, to try moving left or right at random
						if (j)
						{
							// Not quite the gravity direction
							// Gravity direction + last change in gravity direction
							// This makes liquid movement a bit less frothy, particularly for balls of liquid in radial gravity. With radial gravity, instead of just moving along a tangent, the attempted movement will follow the curvature a bit better.
							nxf += r*(pGravY*2.0f-prev_pGravY);
							nyf += -r*(pGravX*2.0f-prev_pGravX);
						}
						else
						{
							nxf += r*pGravY;
							nyf += -r*pGravX;
						}
						prev_pGravX = pGravX;
						prev_pGravY = pGravY;
						// Check whether movement is allowed
						nx = (int)(nxf+0.5f);
						ny = (int)(nyf+0.5f);
						if (nx<0 || ny<0 || nx>=XRES || ny >=YRES)
							break;
						if (TYP(pmap[ny][nx]) != t || bmap[ny/CELL][nx/CELL])
						{
							s = DoMove(i, x, y, nxf, nyf);
							if (s)
							{
								// Movement was successful
								nx = (int)(parts[i].x+0.5f);
								ny = (int)(parts[i].y+0.5f);
								break;
							}
							// A particle of a different type, or a wall, was found. Stop trying to move any further horizontally unless the wall should be completely invisible to particles.
							if (TYP(pmap[ny][nx]) != t || bmap[ny/CELL][nx/CELL] != WL_STREAM)
								break;
						}
					}
					if (s == 1)
					{
						// The particle managed to move horizontally, now try to move vertically (parallel to gravity direction)
						// Keep going until the particle is blocked (by something that isn't the same element) or the number of positions examined = rt
						clear_x = nx;
						clear_y = ny;
						for (int j = 0; j < rt; j++)
						{
							// Calculate overall gravity direction
							GetGravityField(nx, ny, ptGrav, 1.0f, pGravX, pGravY);
							// Scale gravity vector so that the largest component is 1 pixel
							mv = tpt::max(fabsf(pGravX), fabsf(pGravY));
							if (mv<0.0001f) break;
							pGravX /= mv;
							pGravY /= mv;
							// Move 1 pixel in the direction of gravity
							nxf += pGravX;
							nyf += pGravY;
							nx = (int)(nxf+0.5f);
							ny = (int)(nyf+0.5f);
							if (nx<0 || ny<0 || nx>=XRES || ny>=YRES)
								break;
							// If the space is anything except the same element (a wall, empty space, or occupied by a particle of a different element), try to move into it
							if (TYP(pmap[ny][nx]) != t || bmap[ny/CELL][nx/CELL])
							{
								s = DoMove(i, clear_x, clear_y, nxf, nyf);
								if (s || TYP(pmap[ny][nx]) != t || bmap[ny/CELL][nx/CELL] != WL_STREAM)
									break; // found the edge of the liquid and movement into it succeeded, so stop moving down
							}
						}
					}
					else if (s==-1) {} // particle is out of bounds
					else if ((clear_x!=x || clear_y!=y) && DoMove(i, x, y, clear_xf, clear_yf)) {} // try moving to the last clear position
					else parts[i].flags |= FLAG_STAGNANT;
					parts[i].vx *= elements[t].Collision;
					parts[i].vy *= elements[t].Collision;
				}
				else
				{
					// if interpolation was done, try moving to last clear position
					if ((clear_x!=x || clear_y!=y) && DoMove(i, x, y, clear_xf, clear_yf)) {}
					else parts[i].flags |= FLAG_STAGNANT;
					parts[i].vx *= elements[t].Collision;
					parts[i].vy *= elements[t].Collision;
				}
			}
		}
	}
	return false;
}

bool Simulation::flood_water(int x, int y, int i)
{
	int x1, x2, originalX = x, originalY = y;
	int r = pmap[y][x];
	if (!r)
		return false;

	// Bitmap for checking where we've already looked
	auto bitmapPtr = std::unique_ptr<char[]>(new char[XRES * YRES]);
	char *bitmap = bitmapPtr.get();
	std::fill(&bitmap[0], &bitmap[XRES * YRES], 0);

	try
	{
		CoordStack& cs = getCoordStackSingleton();
		cs.push(x, y);
		do
		{
			cs.pop(x, y);
			x1 = x2 = x;
			while (x1 >= CELL)
			{
				if (elements[TYP(pmap[y][x1 - 1])].Falldown != 2 || bitmap[(y * XRES) + x1 - 1])
					break;
				x1--;
			}
			while (x2 < XRES-CELL)
			{
				if (elements[TYP(pmap[y][x2 + 1])].Falldown != 2 || bitmap[(y * XRES) + x1 - 1])
					break;
				x2++;
			}
			for (int x = x1; x <= x2; x++)
			{
				// Check above, maybe around other sides too?
				if (((y - 1) > originalY) && !pmap[y - 1][x])
				{
					// Try to move the water to a random position on this line, because there's probably a free location somewhere
					int randPos = RNG::Ref().between(x, x2);
					if (!pmap[y - 1][randPos] && EvalMove(parts[i].type, randPos, y - 1, nullptr))
						x = randPos;
					// Couldn't move to random position, so try the original position on the left
					else if (!EvalMove(parts[i].type, x, y - 1, nullptr))
						continue;

					Move(i, originalX, originalY, x, y - 1);
					return true;
				}

				bitmap[(y * XRES) + x] = 1;
			}
			if (y >= CELL + 1)
				for (int x = x1; x <= x2; x++)
					if (elements[TYP(pmap[y - 1][x])].Falldown == 2 && !bitmap[((y - 1) * XRES) + x])
						cs.push(x, y - 1);
			if (y < YRES - CELL - 1)
				for (int x = x1; x <= x2; x++)
					if (elements[TYP(pmap[y + 1][x])].Falldown == 2 && !bitmap[((y + 1) * XRES) + x])
						cs.push(x, y + 1);
		} while (cs.getSize() > 0);
	}
	catch (CoordStackOverflowException& e)
	{
		std::cerr << e.what() << std::endl;
		return false;
	}
	return false;
}

/* spark_conductive turns a particle into SPRK and sets ctype, life, and temperature.
 * spark_all does something similar, but behaves correctly for WIRE and INST
 *
 * spark_conductive_attempt and spark_all_attempt do the same thing, except they check whether the particle can actually be sparked (is conductive, has life of zero) first. Remember to check for INSL though. 
 * They return true if the particle was successfully sparked.
*/

void Simulation::spark_all(int i, int x, int y)
{
	if (parts[i].type==PT_WIRE)
		parts[i].ctype = PT_DUST;
	//else if (parts[i].type==PT_INST)
	//	INST_flood_spark(this, x, y);
	else
		spark_conductive(i, x, y);
}
void Simulation::spark_conductive(int i, int x, int y)
{
	int type = parts[i].type;
	part_change_type(i, x, y, PT_SPRK);
	parts[i].ctype = type;
	if (type==PT_WATR)
		parts[i].life = 6;
	else if (type==PT_SLTW)
		parts[i].life = 5;
	else
		parts[i].life = 4;
	if (parts[i].temp < 673.0f && !legacy_enable && (type==PT_METL || type == PT_BMTL || type == PT_BRMT || type == PT_PSCN || type == PT_NSCN || type == PT_ETRD || type == PT_NBLE || type == PT_IRON))
	{
		parts[i].temp = parts[i].temp+10.0f;
		if (parts[i].temp > 673.0f)
			parts[i].temp = 673.0f;
	}
}
bool Simulation::spark_all_attempt(int i, int x, int y)
{
	if ((parts[i].type==PT_WIRE && parts[i].ctype<=0) || (parts[i].type==PT_INST && parts[i].life<=0))
	{
		spark_all(i, x, y);
		return true;
	}
	else if (!parts[i].life && (elements[parts[i].type].Properties & PROP_CONDUCTS))
	{
		spark_conductive(i, x, y);
		return true;
	}
	return false;
}
bool Simulation::spark_conductive_attempt(int i, int x, int y)
{
	if (!parts[i].life && (elements[parts[i].type].Properties & PROP_CONDUCTS))
	{
		spark_conductive(i, x, y);
		return true;
	}
	return false;
}

/*
 *
 *Functions for creating parts, walls, tools, and deco
 *
 */

int Simulation::CreateParts(int x, int y, int c, int flags, bool fill, Brush* brush)
{
	int f = 0, rx = 0, ry = 0, shape = CIRCLE_BRUSH;
	if (brush)
	{
		rx = brush->GetRadius().X, ry = brush->GetRadius().Y, shape = brush->GetShape();
	}

	if (c == PT_LIGH)
	{
		if (lightningRecreate > 0)
			return 0;
		int newlife = rx + ry;
		if (newlife > 55)
			newlife = 55;
		c = PMAP(newlife, c);
		lightningRecreate = std::max(newlife / 4, 1);
		rx = ry = 0;
	}
	else if (c == PT_STKM || c == PT_STKM2 || c == PT_FIGH)
		rx = ry = 0;
	else if (c == PT_TESC)
	{
		int newtmp = (rx*4+ry*4+7);
		if (newtmp > 300)
			newtmp = 300;
		c = PMAP(newtmp, PT_TESC);
	}
#ifndef NOMOD
	else if (c == PT_MOVS)
	{
		if (CreatePartFlags(x, y, PMAP(1, c), flags) && !static_cast<MOVS_ElementDataContainer&>(*elementData[PT_MOVS]).IsCreatingSolid())
			return 1;
		c = PMAP(2, c);
	}
#endif

	if (rx<=0) //workaround for rx == 0 crashing. todo: find a better fix later.
	{
		for (int j = y - ry; j <= y + ry; j++)
			if (CreatePartFlags(x, j, c, flags))
				f = 1;
	}
	else
	{
		int tempy = y - 1, i, j, jmax, oldy;
		// tempy is the closest y value that is just outside (above) the brush
		// jmax is the closest y value that is just outside (below) the brush

		// For triangle brush, start at the very bottom
		if (shape == TRI_BRUSH)
			tempy = y + ry;
		for (i = x - rx; i <= x; i++)
		{
			oldy = tempy;
			while (brush && tempy >= y - ry && brush->IsInside(i - x, tempy - y))
				tempy = tempy - 1;
			if (fill)
			{
				// If triangle brush, create parts down to the bottom always; if not go down to the bottom border
				if (shape == TRI_BRUSH)
					jmax = y + ry;
				else
					jmax = 2 * y - tempy;

				for (j = jmax - 1; j > tempy; j--)
				{
					if (CreatePartFlags(i, j, c, flags))
						f = 1;
					// Don't create twice in the vertical center line
					if (i!=x && CreatePartFlags(2*x-i, j, c, flags))
						f = 1;
				}
			}
			else
			{
				for (j = oldy + 1; j > tempy; j--)
				{
					int i2 = 2*x-i, j2 = 2*y-j;
					if (shape == TRI_BRUSH)
						j2 = y+ry;
					if (CreatePartFlags(i, j, c, flags))
						f = 1;
					if (i2 != i && CreatePartFlags(i2, j, c, flags))
						f = 1;
					if (j2 != j && CreatePartFlags(i, j2, c, flags))
						f = 1;
					if (i2 != i && j2 != j && CreatePartFlags(i2, j2, c, flags))
						f = 1;
				}
			}
		}
	}
	return !f;
}

int Simulation::CreatePartFlags(int x, int y, int c, int flags)
{
	if (x < 0 || y < 0 || x >= XRES || y >= YRES)
	{
		return 0;
	}

	// Specific Delete
	if ((flags & BRUSH_SPECIFIC_DELETE) && !(flags & BRUSH_REPLACEMODE))
	{
		int replaceModeSelected = ((ElementTool*)activeTools[2])->GetID();
		if ((replaceModeSelected <= 0 && (pmap[y][x] || photons[y][x]))
			|| (!photons[y][x] && pmap[y][x] && (int)TYP(pmap[y][x]) == replaceModeSelected)
			|| (photons[y][x] && (int)TYP(photons[y][x]) == replaceModeSelected))
			part_delete(x, y);
	}
	// Replace Mode
	else if (flags & BRUSH_REPLACEMODE)
	{
		int replaceModeSelected = ((ElementTool*)activeTools[2])->GetID();
		if ((replaceModeSelected <= 0 && (pmap[y][x] || photons[y][x]))
			|| (!photons[y][x] && pmap[y][x] && (int)TYP(pmap[y][x]) == replaceModeSelected)
			|| (photons[y][x] && (int)TYP(photons[y][x]) == replaceModeSelected))
		{
			part_delete(x, y);
			if (c != 0)
				part_create(-2, x, y, TYP(c), ID(c));
		}
	}
	// Delete
	else if (c == 0)
	{
		part_delete(x, y);
	}
	else
	{
		// Normal draw
		return (part_create(-2, x, y, TYP(c), ID(c)) == -1);
	}

	return 0;
}

void Simulation::CreateLine(int x1, int y1, int x2, int y2, int c, int flags, Brush* brush)
{
	int x, y, dx, dy, sy;
	bool reverseXY = abs(y2-y1) > abs(x2-x1), fill = true;
	float e = 0.0f, de;
	if (reverseXY)
	{
		y = x1;
		x1 = y1;
		y1 = y;
		y = x2;
		x2 = y2;
		y2 = y;
	}
	if (x1 > x2)
	{
		y = x1;
		x1 = x2;
		x2 = y;
		y = y1;
		y1 = y2;
		y2 = y;
	}
	dx = x2 - x1;
	dy = abs(y2 - y1);
	de = dx ? dy/(float)dx : 0.0f;
	y = y1;
	sy = (y1<y2) ? 1 : -1;
	for (x=x1; x<=x2; x++)
	{
		if (reverseXY)
			CreateParts(y, x, c, flags, fill, brush);
		else
			CreateParts(x, y, c, flags, fill, brush);
		e += de;
		fill = false;
		if (e >= 0.5f)
		{
			y += sy;
			if (!(brush && brush->GetRadius().X+brush->GetRadius().Y) && ((y1<y2) ? (y<=y2) : (y>=y2)))
			{
				if (reverseXY)
					CreateParts(y, x, c, flags, fill, brush);
				else
					CreateParts(x, y, c, flags, fill, brush);
			}
			e -= 1.0f;
		}
	}
}

void Simulation::CreateBox(int x1, int y1, int x2, int y2, int c, int flags)
{
	if (x1 > x2)
	{
		int temp = x2;
		x2 = x1;
		x1 = temp;
	}
	if (y1 > y2)
	{
		int temp = y2;
		y2 = y1;
		y1 = temp;
	}
	for (int j = y2; j >= y1; j--)
		for (int i = x1; i <= x2; i++)
			CreateParts(i, j, c, flags, false);
}

//used for element and prop tool floodfills
int Simulation::FloodFillPmapCheck(int x, int y, unsigned int type)
{
	if (type == 0)
		return !pmap[y][x] && !photons[y][x];
	if (elements[type].Properties&TYPE_ENERGY)
		return TYP(photons[y][x]) == type;
	else
		return TYP(pmap[y][x]) == type;
}

int Simulation::FloodParts(int x, int y, int fullc, int replace, int flags)
{
	unsigned int c = TYP(fullc);
	int x1, x2;
	int created_something = 0;

	if (replace == -1)
	{
		//if initial flood point is out of bounds, do nothing
		if (c != 0 && (x < CELL || x >= XRES-CELL || y < CELL || y >= YRES-CELL || c == PT_SPRK))
			return 1;
		if (c == 0)
		{
			replace = TYP(pmap[y][x]);
			if (!replace && !(replace = TYP(photons[y][x])))
			{
				if (bmap[y/CELL][x/CELL])
					return FloodWalls(x/CELL, y/CELL, WL_ERASE, -1);
				else
					return 0;
				//if ((flags&BRUSH_REPLACEMODE) && ((ElementTool*)activeTools[2])->GetID() != replace)
				//	return 0;
			}
		}
		else
			replace = 0;
	}
	if (c != 0 && IsWallBlocking(x, y, c))
		return 0;

	if (!FloodFillPmapCheck(x, y, replace) || ((flags&BRUSH_SPECIFIC_DELETE) && ((ElementTool*)activeTools[2])->GetID() != replace))
		return 0;

	// Bitmap for checking where we've already looked
	auto bitmapPtr = std::unique_ptr<char[]>(new char[XRES * YRES]);
	char *bitmap = bitmapPtr.get();
	std::fill(&bitmap[0], &bitmap[XRES * YRES], 0);

	try
	{
		CoordStack& cs = getCoordStackSingleton();
		cs.push(x, y);

		do
		{
			cs.pop(x, y);
			x1 = x2 = x;
			// go left as far as possible
			while (c?x1>CELL:x1>0)
			{
				if (bitmap[(y * XRES) + x1 - 1] || !FloodFillPmapCheck(x1-1, y, replace) || (c != 0 && IsWallBlocking(x1-1, y, c)))
				{
					break;
				}
				x1--;
			}
			// go right as far as possible
			while (c?x2<XRES-CELL-1:x2<XRES-1)
			{
				if (bitmap[(y * XRES) + x2 + 1] || !FloodFillPmapCheck(x2+1, y, replace) || (c != 0 && IsWallBlocking(x2+1, y, c)))
				{
					break;
				}
				x2++;
			}
			// fill span
			for (x=x1; x<=x2; x++)
			{
				if (!fullc)
				{
					if (elements[replace].Properties&TYPE_ENERGY)
					{
						if (photons[y][x])
						{
							part_kill(ID(photons[y][x]));
							created_something = 1;
						}
					}
					else if (pmap[y][x])
					{
						part_kill(ID(pmap[y][x]));
						created_something = 1;
					}
				}
				else if (CreateParts(x, y, fullc, flags, true))
					created_something = 1;

				bitmap[(y * XRES) + x] = 1;
			}

			if (c ? y>CELL : y>0)
				for (x=x1; x<=x2; x++)
					if (!bitmap[((y - 1) * XRES) + x] && FloodFillPmapCheck(x, y-1, replace) && (c == 0 || !IsWallBlocking(x, y-1, c)))
					{
						cs.push(x, y-1);
					}

			if (c ? y<YRES-CELL-1 : y<YRES-1)
				for (x=x1; x<=x2; x++)
					if (!bitmap[((y + 1) * XRES) + x] && FloodFillPmapCheck(x, y+1, replace) && (c == 0 || !IsWallBlocking(x, y+1, c)))
					{
						cs.push(x, y+1);
					}
		}
		while (cs.getSize() > 0);
	}
	catch (const CoordStackOverflowException& e)
	{
		(void)e; //ignore compiler warning
		return -1;
	}
	return created_something;
}

void Simulation::CreateWall(int x, int y, int wall)
{
	if (x < 0 || y < 0 || x >= XRES/CELL || y >= YRES/CELL)
		return;

	//reset fan velocity, as if new fan walls had been created
	if (wall == WL_FAN)
	{
		air->fvx[y][x] = 0.0f;
		air->fvy[y][x] = 0.0f;
	}
	//streamlines can't be drawn next to each other
	else if (wall == WL_STREAM)
	{
		for (int tempY = y-1; tempY <= y+1; tempY++)
			for (int tempX = x-1; tempX <= x+1; tempX++)
			{
				if (tempX >= 0 && tempX < XRES/CELL && tempY >= 0 && tempY < YRES/CELL && bmap[tempY][tempX] == WL_STREAM)
					return;
			}
	}
	else if (wall == WL_ERASEALL)
	{
		for (int i = 0; i < CELL; i++)
			for (int j = 0; j < CELL; j++)
			{
				part_delete(x*CELL+i, y*CELL+j);
			}
		DeleteSignsInArea(Point(x, y)*CELL, Point(x+1, y+1)*CELL);
		wall = 0;
	}
	if (wall == WL_GRAV || bmap[y][x] == WL_GRAV)
		grav->gravWallChanged = true;
	bmap[y][x] = (unsigned char)wall;
}

void Simulation::CreateWallLine(int x1, int y1, int x2, int y2, int rx, int ry, int wall)
{
	int x, y, dx, dy, sy;
	bool reverseXY = abs(y2-y1) > abs(x2-x1);
	float e = 0.0f, de;
	if (reverseXY)
	{
		y = x1;
		x1 = y1;
		y1 = y;
		y = x2;
		x2 = y2;
		y2 = y;
	}
	if (x1 > x2)
	{
		y = x1;
		x1 = x2;
		x2 = y;
		y = y1;
		y1 = y2;
		y2 = y;
	}
	dx = x2 - x1;
	dy = abs(y2 - y1);
	de = dx ? dy/(float)dx : 0.0f;
	y = y1;
	sy = (y1<y2) ? 1 : -1;
	for (x=x1; x<=x2; x++)
	{
		if (reverseXY)
			CreateWallBox(y-rx, x-ry, y+rx, x+ry, wall);
		else
			CreateWallBox(x-rx, y-ry, x+rx, y+ry, wall);
		e += de;
		if (e >= 0.5f)
		{
			y += sy;
			if ((y1<y2) ? (y<=y2) : (y>=y2))
			{
				if (reverseXY)
					CreateWallBox(y-rx, x-ry, y+rx, x+ry, wall);
				else
					CreateWallBox(x-rx, y-ry, x+rx, y+ry, wall);
			}
			e -= 1.0f;
		}
	}
}

void Simulation::CreateWallBox(int x1, int y1, int x2, int y2, int wall)
{
	if (x1 > x2)
	{
		int temp = x2;
		x2 = x1;
		x1 = temp;
	}
	if (y1 > y2)
	{
		int temp = y2;
		y2 = y1;
		y1 = temp;
	}
	for (int j = y1; j <= y2; j++)
		for (int i = x1; i <= x2; i++)
			CreateWall(i, j, wall);
}

int Simulation::FloodWalls(int x, int y, int wall, int replace)
{
	int x1, x2;
	if (replace == -1)
	{
		if (wall==WL_ERASE || wall==WL_ERASEALL)
		{
			replace = bmap[y][x];
			if (!replace)
				return 0;
		}
		else
			replace = 0;
	}

	if (bmap[y][x] != replace)
		return 1;

	// go left as far as possible
	x1 = x2 = x;
	while (x1 >= 1)
	{
		if (bmap[y][x1-1] != replace)
		{
			break;
		}
		x1--;
	}
	while (x2 < XRES/CELL-1)
	{
		if (bmap[y][x2+1] != replace)
		{
			break;
		}
		x2++;
	}

	// fill span
	for (x=x1; x<=x2; x++)
	{
		CreateWall(x, y, wall);
	}
	// fill children
	if (y >= 1)
		for (x=x1; x<=x2; x++)
			if (bmap[y-1][x] == replace)
				if (!FloodWalls(x, y-1, wall, replace))
					return 0;
	if (y < YRES/CELL-1)
		for (x=x1; x<=x2; x++)
			if (bmap[y+1][x] == replace)
				if (!FloodWalls(x, y+1, wall, replace))
					return 0;
	return 1;
}

int Simulation::CreateTool(int x, int y, int brushX, int brushY, int tool, float strength)
{
	if (!InBounds(x, y))
		return -2;
	if (tool == TOOL_HEAT || tool == TOOL_COOL)
	{
		int r = pmap[y][x];
		if (!TYP(r))
			r = photons[y][x];
		if TYP(r)
		{
			float heatchange;
			if (TYP(r) == PT_PUMP || TYP(r) == PT_GPMP)
				heatchange = strength*.1f;
#ifndef NOMOD
			else if (TYP(r) == PT_ANIM)
				heatchange = strength;
#endif
			else
				heatchange = strength*2.0f;

			if (tool == TOOL_HEAT)
			{
				parts[ID(r)].temp = restrict_flt(parts[ID(r)].temp + heatchange, MIN_TEMP, MAX_TEMP);
			}
			else if (tool == TOOL_COOL)
			{
				parts[ID(r)].temp = restrict_flt(parts[ID(r)].temp - heatchange, MIN_TEMP, MAX_TEMP);
			}
			return ID(r);
		}
		else
		{
			return -1;
		}
	}
	else if (tool == TOOL_AIR)
	{
		air->pv[y/CELL][x/CELL] += strength*.05f;
		return -1;
	}
	else if (tool == TOOL_VAC)
	{
		air->pv[y/CELL][x/CELL] -= strength*.05f;
		return -1;
	}
	else if (tool == TOOL_PGRV)
	{
		grav->gravmap[(y/CELL)*(XRES/CELL)+(x/CELL)] = strength*5.0f;
		return -1;
	}
	else if (tool == TOOL_NGRV)
	{
		grav->gravmap[(y/CELL)*(XRES/CELL)+(x/CELL)] = strength*-5.0f;
		return -1;
	}
	else if (tool == TOOL_MIX)
	{
		int thisPart = pmap[y][x];
		if (!thisPart)
			return 0;

		if (RNG::Ref().chance(1, 100))
			return 0;

		int distance = (int)(std::pow(strength, .5f) * 10);

		if (!(elements[TYP(thisPart)].Properties & (TYPE_PART | TYPE_LIQUID | TYPE_GAS)))
			return 0;

		int newX = x + RNG::Ref().between(0, distance - 1) - (distance/2);
		int newY = y + RNG::Ref().between(0, distance - 1) - (distance/2);

		if(newX < 0 || newY < 0 || newX >= XRES || newY >= YRES)
			return 0;

		int thatPart = pmap[newY][newX];
		if(!thatPart)
			return 0;

		if ((elements[TYP(thisPart)].Properties&STATE_FLAGS) != (elements[TYP(thatPart)].Properties&STATE_FLAGS))
			return 0;

		pmap[y][x] = thatPart;
		parts[ID(thatPart)].x = x;
		parts[ID(thatPart)].y = y;

		pmap[newY][newX] = thisPart;
		parts[ID(thisPart)].x = newX;
		parts[ID(thisPart)].y = newY;
		return -1;
	}
	else if (tool == TOOL_CYCL)
	{
		/* 
			Air velocity calculation.
			(x, y) -- turn 90 deg -> (-y, x)
		*/
		// only trigger once per cell (less laggy)
		if ((x%CELL) == 0 && (y%CELL) == 0 && !(brushX == x && brushY == y))
		{
			float *vx = &air->vx[y/CELL][x/CELL];
			float *vy = &air->vy[y/CELL][x/CELL];

			float dvx = brushX - x;
			float dvy = brushY - y;
			float invsqr = 1 / sqrtf(dvx * dvx + dvy * dvy);

			*vx -= (strength / 16) * (-dvy) * invsqr;
			*vy -= (strength / 16) * dvx * invsqr;

			// Clamp velocities
			if (*vx > 256.0f)
				*vx = 256.0f;
			else if (*vx < -256.0f)
				*vx = -256.0f;
			if (*vy > 256.0f)
				*vy = 256.0f;
			else if (*vy < -256.0f)
				*vy = -256.0f;
		}
		return -1;
	}
	else if (tool == TOOL_AMBM)
	{
		if (!aheat_enable)
		{
			return 0;
		}

		air->hv[y / CELL][x / CELL] -= strength * 2.0f;
		if (air->hv[y / CELL][x / CELL] > MAX_TEMP) air->hv[y / CELL][x / CELL] = MAX_TEMP;
		if (air->hv[y / CELL][x / CELL] < MIN_TEMP) air->hv[y / CELL][x / CELL] = MIN_TEMP;
		return 1;
	}
	else if (tool == TOOL_AMBP)
	{
		if (!aheat_enable)
		{
			return 0;
		}

		air->hv[y / CELL][x / CELL] += strength * 2.0f;
		if (air->hv[y / CELL][x / CELL] > MAX_TEMP) air->hv[y / CELL][x / CELL] = MAX_TEMP;
		if (air->hv[y / CELL][x / CELL] < MIN_TEMP) air->hv[y / CELL][x / CELL] = MIN_TEMP;
		return 1;
	}
	return -1;
}

void Simulation::CreateToolBrush(int x, int y, int tool, float strength, Brush* brush)
{
	int rx = brush->GetRadius().X, ry = brush->GetRadius().Y;
	if (rx <= 0) //workaround for rx == 0 crashing. todo: find a better fix later.
	{
		for (int j = y - ry; j <= y + ry; j++)
			CreateTool(x, j, x, y, tool, strength);
	}
	else
	{
		int tempy = y - 1, i, j, jmax;
		// tempy is the closest y value that is just outside (above) the brush
		// jmax is the closest y value that is just outside (below) the brush

		// For triangle brush, start at the very bottom
		if (brush->GetShape() == TRI_BRUSH)
			tempy = y + ry;
		for (i = x - rx; i <= x; i++)
		{
			//loop up until it finds a point not in the brush
			while (tempy >= y - ry && brush->IsInside(i - x, tempy - y))
				tempy = tempy - 1;

			// If triangle brush, create parts down to the bottom always; if not go down to the bottom border
			if (brush->GetShape() == TRI_BRUSH)
				jmax = y + ry;
			else
				jmax = 2 * y - tempy;
			for (j = tempy + 1; j < jmax; j++)
			{
				CreateTool(i, j, x, y, tool, strength);
				// Don't create twice in the vertical center line
				if (i != x)
					CreateTool(2*x-i, j, x, y, tool, strength);
			}
		}
	}
}

void Simulation::CreateToolLine(int x1, int y1, int x2, int y2, int tool, float strength, Brush* brush)
{
	if (tool == TOOL_WIND)
	{
		int rx = brush->GetRadius().X, ry = brush->GetRadius().Y;
		for (int j = -ry; j <= ry; j++)
			for (int i = -rx; i <= rx; i++)
				if (x1+i>0 && y1+j>0 && x1+i<XRES && y1+j<YRES && brush->IsInside(i, j))
				{
					air->vx[(y1+j)/CELL][(x1+i)/CELL] += (x2-x1)*strength;
					air->vy[(y1+j)/CELL][(x1+i)/CELL] += (y2-y1)*strength;
				}
		return;
	}
	int x, y, dx, dy, sy;
	bool reverseXY = abs(y2-y1) > abs(x2-x1);
	float e = 0.0f, de;
	if (reverseXY)
	{
		y = x1;
		x1 = y1;
		y1 = y;
		y = x2;
		x2 = y2;
		y2 = y;
	}
	if (x1 > x2)
	{
		y = x1;
		x1 = x2;
		x2 = y;
		y = y1;
		y1 = y2;
		y2 = y;
	}
	dx = x2 - x1;
	dy = abs(y2 - y1);
	de = dx ? dy/(float)dx : 0.0f;
	y = y1;
	sy = (y1<y2) ? 1 : -1;
	for (x=x1; x<=x2; x++)
	{
		if (reverseXY)
			CreateToolBrush(y, x, tool, strength, brush);
		else
			CreateToolBrush(x, y, tool, strength, brush);
		e += de;
		if (e >= 0.5f)
		{
			y += sy;
			if (!(brush->GetRadius().X+brush->GetRadius().Y) && ((y1<y2) ? (y<=y2) : (y>=y2)))
			{
				if (reverseXY)
					CreateToolBrush(y, x, tool, strength, brush);
				else
					CreateToolBrush(x, y, tool, strength, brush);
			}
			e -= 1.0f;
		}
	}
}

void Simulation::CreateToolBox(int x1, int y1, int x2, int y2, int tool, float strength)
{
	int brushX = ((x1 + x2) / 2);
	int brushY = ((y1 + y2) / 2);
	if (x1 > x2)
	{
		int temp = x2;
		x2 = x1;
		x1 = temp;
	}
	if (y1 > y2)
	{
		int temp = y2;
		y2 = y1;
		y1 = temp;
	}
	for (int j = y1; j <= y2; j++)
		for (int i = x1; i <= x2; i++)
			CreateTool(i, j, brushX, brushY, tool, strength);
}

int Simulation::CreateProp(int x, int y, StructProperty prop, PropertyValue propValue)
{
	if (!InBounds(x, y))
		return -2;

	int i = pmap[y][x];
	if (!i)
		i = photons[y][x];

	if (TYP(i))
	{
		if (prop.Name == "Type")
			part_change_type_force(i, propValue.Integer);
		else if (prop.Type == StructProperty::Integer || prop.Type == StructProperty::ParticleType)
			*(reinterpret_cast<int*>((reinterpret_cast<char*>(&parts[ID(i)])) + prop.Offset)) = propValue.Integer;
		else if (prop.Type == StructProperty::UInteger)
			*(reinterpret_cast<unsigned int*>((reinterpret_cast<char*>(&parts[ID(i)])) + prop.Offset)) = propValue.UInteger;
		else if (prop.Type == StructProperty::Float)
			*(reinterpret_cast<float*>((reinterpret_cast<char*>(&parts[ID(i)])) + prop.Offset)) = propValue.Float;
		return ID(i);
	}
	return -1;
}

void Simulation::CreatePropBrush(int x, int y, StructProperty prop, PropertyValue propValue, Brush *brush)
{
	int rx = brush->GetRadius().X, ry = brush->GetRadius().Y;
	if (rx <= 0) //workaround for rx == 0 crashing. todo: find a better fix later.
	{
		for (int j = y - ry; j <= y + ry; j++)
			CreateProp(x, j, prop, propValue);
	}
	else
	{
		int tempy = y - 1, i, j, jmax;
		// tempy is the closest y value that is just outside (above) the brush
		// jmax is the closest y value that is just outside (below) the brush

		// For triangle brush, start at the very bottom
		if (brush->GetShape() == TRI_BRUSH)
			tempy = y + ry;
		for (i = x - rx; i <= x; i++)
		{
			//loop up until it finds a point not in the brush
			while (tempy >= y - ry && brush->IsInside(i - x, tempy - y))
				tempy = tempy - 1;

			// If triangle brush, create parts down to the bottom always; if not go down to the bottom border
			if (brush->GetShape() == TRI_BRUSH)
				jmax = y + ry;
			else
				jmax = 2 * y - tempy;
			for (j = tempy + 1; j < jmax; j++)
			{
				CreateProp(i, j, prop, propValue);
				// Don't create twice in the vertical center line
				if (i != x)
					CreateProp(2 * x - i, j, prop, propValue);
			}
			//TODO: should use fill argument like creating elements does (but all the repetitiveness here needs to be removed first)
		}
	}
}

void Simulation::CreatePropLine(int x1, int y1, int x2, int y2, StructProperty prop, PropertyValue propValue, Brush *brush)
{
	int x, y, dx, dy, sy;
	bool reverseXY = abs(y2 - y1) > abs(x2 - x1);
	float e = 0.0f, de;
	if (reverseXY)
	{
		y = x1;
		x1 = y1;
		y1 = y;
		y = x2;
		x2 = y2;
		y2 = y;
	}
	if (x1 > x2)
	{
		y = x1;
		x1 = x2;
		x2 = y;
		y = y1;
		y1 = y2;
		y2 = y;
	}
	dx = x2 - x1;
	dy = abs(y2 - y1);
	de = dx ? dy/(float)dx : 0.0f;
	y = y1;
	sy = (y1<y2) ? 1 : -1;
	for (x = x1; x <= x2; x++)
	{
		if (reverseXY)
			CreatePropBrush(y, x, prop, propValue, brush);
		else
			CreatePropBrush(x, y, prop, propValue, brush);
		e += de;
		if (e >= 0.5f)
		{
			y += sy;
			if (!(brush->GetRadius().X +  brush->GetRadius().Y) && ((y1<y2) ? (y <= y2) : (y >= y2)))
			{
				if (reverseXY)
					CreatePropBrush(y, x, prop, propValue, brush);
				else
					CreatePropBrush(x, y, prop, propValue, brush);
			}
			e -= 1.0f;
		}
	}
}

void Simulation::CreatePropBox(int x1, int y1, int x2, int y2, StructProperty prop, PropertyValue propValue)
{
	if (x1 > x2)
	{
		int temp = x2;
		x2 = x1;
		x1 = temp;
	}
	if (y1 > y2)
	{
		int temp = y2;
		y2 = y1;
		y1 = temp;
	}
	for (int j = y1; j <= y2; j++)
		for (int i = x1; i <= x2; i++)
			CreateProp(i, j, prop, propValue);
}

int Simulation::FloodProp(int x, int y, StructProperty prop, PropertyValue propValue)
{
	int x1, x2, dy = 1;
	int did_something = 0;
	int r = pmap[y][x];
	if (!r)
		r = photons[y][x];
	if (!r)
		return 0;
	int parttype = TYP(r);
	char *bitmap = new char[XRES*YRES]; //Bitmap for checking
	if (!bitmap)
		return -1;
	std::fill_n(&bitmap[0], XRES*YRES, 0);
	try
	{
		CoordStack& cs = getCoordStackSingleton();
		cs.push(x, y);
		do
		{
			cs.pop(x, y);
			x1 = x2 = x;
			while (x1>=CELL)
			{
				if (!FloodFillPmapCheck(x1-1, y, parttype) || bitmap[(y*XRES)+x1-1])
					break;
				x1--;
			}
			while (x2<XRES-CELL)
			{
				if (!FloodFillPmapCheck(x2+1, y, parttype) || bitmap[(y*XRES)+x2+1])
					break;
				x2++;
			}
			for (x=x1; x<=x2; x++)
			{
				CreateProp(x, y, prop, propValue);
				bitmap[(y*XRES)+x] = 1;
				did_something = 1;
			}
			if (y>=CELL+dy)
				for (x=x1; x<=x2; x++)
					if (FloodFillPmapCheck(x, y-dy, parttype) && !bitmap[((y-dy)*XRES)+x])
						cs.push(x, y-dy);
			if (y<YRES-CELL-dy)
				for (x=x1; x<=x2; x++)
					if (FloodFillPmapCheck(x, y+dy, parttype) && !bitmap[((y+dy)*XRES)+x])
						cs.push(x, y+dy);
		} while (cs.getSize()>0);
	}
	catch (CoordStackOverflowException& e)
	{
		std::cerr << e.what() << std::endl;
		delete[] bitmap;
		return -1;
	}
	delete[] bitmap;
	return did_something;
}

void Simulation::CreateDeco(int x, int y, int tool, ARGBColour color)
{
	int tr = 0, tg = 0, tb = 0, ta = 0;
	float strength = 0.01f, colr, colg, colb, cola;
	if (!InBounds(x, y))
		return;

	int rp = pmap[y][x];
	if (!rp)
		rp = photons[y][x];
	if (!rp)
		return;
	rp = ID(rp);

	switch (tool)
	{
	case DECO_DRAW:
		parts[rp].dcolour = color;
		break;
	case DECO_CLEAR:
		parts[rp].dcolour = COLARGB(0, 0, 0, 0);
		break;
	case DECO_ADD:
	case DECO_SUBTRACT:
	case DECO_MULTIPLY:
	case DECO_DIVIDE:
		if (!parts[rp].dcolour)
			return;
		cola = COLA(color)/255.0f;
		colr = (float)COLR(parts[rp].dcolour);
		colg = (float)COLG(parts[rp].dcolour);
		colb = (float)COLB(parts[rp].dcolour);

		if (tool == DECO_ADD)
		{
			colr += (COLR(color)*strength)*cola;
			colg += (COLG(color)*strength)*cola;
			colb += (COLB(color)*strength)*cola;
		}
		else if (tool == DECO_SUBTRACT)
		{
			colr -= (COLR(color)*strength)*cola;
			colg -= (COLG(color)*strength)*cola;
			colb -= (COLB(color)*strength)*cola;
		}
		else if (tool == DECO_MULTIPLY)
		{
			colr *= 1.0f+(COLR(color)/255.0f*strength)*cola;
			colg *= 1.0f+(COLG(color)/255.0f*strength)*cola;
			colb *= 1.0f+(COLB(color)/255.0f*strength)*cola;
		}
		else if (tool == DECO_DIVIDE)
		{
			colr /= 1.0f+(COLR(color)/255.0f*strength)*cola;
			colg /= 1.0f+(COLG(color)/255.0f*strength)*cola;
			colb /= 1.0f+(COLB(color)/255.0f*strength)*cola;
		}

		tr = int(colr+.5f); tg = int(colg+.5f); tb = int(colb+.5f);
		if (tr > 255) tr = 255;
		else if (tr < 0) tr = 0;
		if (tg > 255) tg = 255;
		else if (tg < 0) tg = 0;
		if (tb > 255) tb = 255;
		else if (tb < 0) tb = 0;

		parts[rp].dcolour = COLRGB(tr, tg, tb);
		break;
	case DECO_LIGHTEN:
		if (!parts[rp].dcolour)
			return;
		tr = COLR(parts[rp].dcolour);
		tg = COLG(parts[rp].dcolour);
		tb = COLB(parts[rp].dcolour);
		ta = COLA(parts[rp].dcolour);
		parts[rp].dcolour = COLARGB(ta, clamp_flt(tr+(255-tr)*0.02f+1, 0, 255), clamp_flt(tg+(255-tg)*0.02f+1, 0, 255), clamp_flt(tb+(255-tb)*0.02f+1, 0, 255));
		break;
	case DECO_DARKEN:
		if (!parts[rp].dcolour)
			return;
		tr = COLR(parts[rp].dcolour);
		tg = COLG(parts[rp].dcolour);
		tb = COLB(parts[rp].dcolour);
		ta = COLA(parts[rp].dcolour);
		parts[rp].dcolour = COLARGB(ta, clamp_flt(tr-(tr)*0.02f, 0, 255), clamp_flt(tg-(tg)*0.02f, 0, 255), clamp_flt(tb-(tb)*0.02f, 0, 255));
		break;
	case DECO_SMUDGE:
		if (x >= CELL && x < XRES-CELL && y >= CELL && y < YRES-CELL)
		{
			int num = 0;
			float tr = 0, tg = 0, tb = 0, ta = 0;
			for (int rx = -2; rx <= 2; rx++)
				for (int ry = -2; ry <= 2; ry++)
				{
					if (abs(rx) + abs(ry) > 2 && TYP(pmap[y + ry][x + rx]) && parts[ID(pmap[y + ry][x + rx])].dcolour)
					{
						num++;
						float pa = COLA(parts[ID(pmap[y+ry][x+rx])].dcolour) / 255.0f;
						float pr = COLR(parts[ID(pmap[y+ry][x+rx])].dcolour) / 255.0f;
						float pg = COLG(parts[ID(pmap[y+ry][x+rx])].dcolour) / 255.0f;
						float pb = COLB(parts[ID(pmap[y+ry][x+rx])].dcolour) / 255.0f;

						switch (decoSpace)
						{
						case DECOSPACE_SRGB:
							pa = (pa <= 0.04045f) ? (pa / 12.92f) : pow((pa + 0.055f) / 1.055f, 2.4f);
							pr = (pr <= 0.04045f) ? (pr / 12.92f) : pow((pr + 0.055f) / 1.055f, 2.4f);
							pg = (pg <= 0.04045f) ? (pg / 12.92f) : pow((pg + 0.055f) / 1.055f, 2.4f);
							pb = (pb <= 0.04045f) ? (pb / 12.92f) : pow((pb + 0.055f) / 1.055f, 2.4f);
							break;
						case DECOSPACE_LINEAR:
							break;
						case DECOSPACE_GAMMA22:
							pa = pow(pa, 2.2f);
							pr = pow(pr, 2.2f);
							pg = pow(pg, 2.2f);
							pb = pow(pb, 2.2f);
							break;
						case DECOSPACE_GAMMA18:
							pa = pow(pa, 1.8f);
							pr = pow(pr, 1.8f);
							pg = pow(pg, 1.8f);
							pb = pow(pb, 1.8f);
							break;
						}

						ta += pa;
						tr += pr;
						tg += pg;
						tb += pb;
					}
				}
			if (num == 0)
				return;

			ta = ta / num;
			tr = tr / num;
			tg = tg / num;
			tb = tb / num;
			switch (decoSpace)
			{
			case DECOSPACE_SRGB:
				ta = (ta <= 0.0031308f) ? (ta * 12.92f) : (1.055f * pow(ta, 1.f / 2.4f) - 0.055f);
				tr = (tr <= 0.0031308f) ? (tr * 12.92f) : (1.055f * pow(tr, 1.f / 2.4f) - 0.055f);
				tg = (tg <= 0.0031308f) ? (tg * 12.92f) : (1.055f * pow(tg, 1.f / 2.4f) - 0.055f);
				tb = (tb <= 0.0031308f) ? (tb * 12.92f) : (1.055f * pow(tb, 1.f / 2.4f) - 0.055f);
				break;
			case DECOSPACE_LINEAR:
				break;
			case DECOSPACE_GAMMA22:
				ta = pow(ta, 1 / 2.2f);
				tr = pow(tr, 1 / 2.2f);
				tg = pow(tg, 1 / 2.2f);
				tb = pow(tb, 1 / 2.2f);
				break;
			case DECOSPACE_GAMMA18:
				ta = pow(ta, 1 / 1.8f);
				tr = pow(tr, 1 / 1.8f);
				tg = pow(tg, 1 / 1.8f);
				tb = pow(tb, 1 / 1.8f);
				break;
			}

			int tai = std::min(255, static_cast<int>(ta * 255.0f + 0.5f));
			int tri = std::min(255, static_cast<int>(tr * 255.0f + 0.5f));
			int tgi = std::min(255, static_cast<int>(tg * 255.0f + 0.5f));
			int tbi = std::min(255, static_cast<int>(tb * 255.0f + 0.5f));

			parts[rp].dcolour = COLARGB(tai, tri, tgi, tbi);
		}
		break;
	}

#ifndef NOMOD
	if (parts[rp].type == PT_ANIM)
		static_cast<ANIM_ElementDataContainer&>(*elementData[PT_ANIM]).SetColor(rp, parts[rp].tmp2, parts[rp].dcolour);
#endif
}

void Simulation::CreateDecoBrush(int x, int y, int tool, ARGBColour color, Brush* brush)
{
	int rx = brush->GetRadius().X, ry = brush->GetRadius().Y;
	if (rx <= 0) //workaround for rx == 0 crashing. todo: find a better fix later.
	{
		for (int j = y - ry; j <= y + ry; j++)
			CreateDeco(x, j, tool, color);
	}
	else
	{
		int tempy = y - 1, i, j, jmax;
		// tempy is the closest y value that is just outside (above) the brush
		// jmax is the closest y value that is just outside (below) the brush

		// For triangle brush, start at the very bottom
		if (brush->GetShape() == TRI_BRUSH)
			tempy = y + ry;
		for (i = x - rx; i <= x; i++)
		{
			//loop up until it finds a point not in the brush
			while (tempy >= y - ry && brush->IsInside(i - x, tempy - y))
				tempy = tempy - 1;

			// If triangle brush, create parts down to the bottom always; if not go down to the bottom border
			if (brush->GetShape() == TRI_BRUSH)
				jmax = y + ry;
			else
				jmax = 2 * y - tempy;

			for (j = tempy + 1; j < jmax; j++)
			{
				CreateDeco(i, j, tool, color);
				// Don't create twice in the vertical center line
				if (i != x)
					CreateDeco(2*x-i, j, tool, color);
			}
		}
	}
}

void Simulation::CreateDecoLine(int x1, int y1, int x2, int y2, int tool, ARGBColour color, Brush* brush)
{
	int x, y, dx, dy, sy;
	bool reverseXY = abs(y2-y1) > abs(x2-x1);
	float e = 0.0f, de;
	if (reverseXY)
	{
		y = x1;
		x1 = y1;
		y1 = y;
		y = x2;
		x2 = y2;
		y2 = y;
	}
	if (x1 > x2)
	{
		y = x1;
		x1 = x2;
		x2 = y;
		y = y1;
		y1 = y2;
		y2 = y;
	}
	dx = x2 - x1;
	dy = abs(y2 - y1);
	de = dx ? dy/(float)dx : 0.0f;
	y = y1;
	sy = (y1<y2) ? 1 : -1;
	for (x=x1; x<=x2; x++)
	{
		if (reverseXY)
			CreateDecoBrush(y, x, tool, color, brush);
		else
			CreateDecoBrush(x, y, tool, color, brush);
		e += de;
		if (e >= 0.5f)
		{
			y += sy;
			if (!(brush->GetRadius().X+brush->GetRadius().Y) && ((y1<y2) ? (y<=y2) : (y>=y2)))
			{
				if (reverseXY)
					CreateDecoBrush(y, x, tool, color, brush);
				else
					CreateDecoBrush(x, y, tool, color, brush);
			}
			e -= 1.0f;
		}
	}
}

void Simulation::CreateDecoBox(int x1, int y1, int x2, int y2, int tool, ARGBColour color)
{
	if (x1 > x2)
	{
		int temp = x2;
		x2 = x1;
		x1 = temp;
	}
	if (y1 > y2)
	{
		int temp = y2;
		y2 = y1;
		y1 = temp;
	}
	for (int j = y1; j <= y2; j++)
		for (int i = x1; i <= x2; i++)
			CreateDeco(i, j, tool, color);
}

bool Simulation::ColorCompare(pixel *vid, int x, int y, ARGBColour replace)
{
	pixel pix = vid[x+y*(XRES+BARSIZE)];
	int r = COLR(replace)-PIXR(pix);
	int g = COLG(replace)-PIXG(pix);
	int b = COLB(replace)-PIXB(pix);
	return (std::abs(r) + std::abs(g) + std::abs(b)) < 15;
}

void Simulation::FloodDeco(pixel *vid, int x, int y, ARGBColour color, ARGBColour replace)
{
	int x1, x2;
	char *bitmap = new char[XRES*YRES]; //Bitmap for checking
	if (!bitmap)
		return;
	std::fill_n(&bitmap[0], XRES*YRES, 0);

	if (!ColorCompare(vid, x, y, replace))
	{
		delete[] bitmap;
		return;
	}

	try
	{
		CoordStack& cs = getCoordStackSingleton();
		cs.push(x, y);
		do
		{
			cs.pop(x, y);
			x1 = x2 = x;
			while (x1 >= 1)
			{
				if (bitmap[(y*XRES)+x1-1] || !ColorCompare(vid, x1-1, y, replace))
					break;
				x1--;
			}
			while (x2 < XRES-1)
			{
				if (bitmap[(y*XRES)+x2+1] || !ColorCompare(vid, x2+1, y, replace))
					break;
				x2++;
			}
			for (x = x1; x <= x2; x++)
			{
				CreateDeco(x, y, DECO_DRAW, color);
				bitmap[(y*XRES)+x] = 1;
			}
			if (y >= 1)
				for (x=x1; x<=x2; x++)
					if (!bitmap[x+(y-1)*XRES] && ColorCompare(vid, x, y-1, replace))
						cs.push(x, y-1);
			if (y < YRES-1)
				for (x=x1; x<=x2; x++)
					if (!bitmap[x+(y+1)*XRES] && ColorCompare(vid, x, y+1, replace))
						cs.push(x, y+1);
		} while (cs.getSize()>0);
	}
	catch (CoordStackOverflowException& e)
	{
		std::cerr << e.what() << std::endl;
		delete[] bitmap;
		return;
	}
	delete[] bitmap;
}

/*
 *
 *End of tool creation section
 *
 */
