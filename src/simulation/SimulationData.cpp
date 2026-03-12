#include "SimulationData.h"
#include "ElementGraphics.h"
#include "ElementDefs.h"
#include "ElementClasses.h"
#include "GOLString.h"
#include "BuiltinGOL.h"
#include "WallType.h"
#include "MenuSection.h"
#include "Misc.h"
#include "common/Localization.h"
#include "graphics/Renderer.h"
#include "simulation/elements/PIPE.h"

const std::array<BuiltinGOL, NGOL> SimulationData::builtinGol = {{
	// * Ruleset:
	//   * bits x = 8..0: stay if x neighbours present
	//   * bits x = 16..9: begin if x-8 neighbours present
	//   * bits 20..17: 4-bit unsigned int encoding the number of states minus 2; 2 states is
	//     encoded as 0, 3 states as 1, etc.
	//   * states are kind of long until a cell dies; normal ones use two states (living and dead),
	//     for others the intermediate states live but do nothing
	//   * the ruleset constants below look 20-bit, but rulesets actually consist of 21
	//     bits of data; bit 20 just happens to not be set for any of the built-in types,
	//     as none of them have 10 or more states
	{ ByteString("GOL"),  String("sim.gol.GOL.name"),  String("sim.gol.GOL.desc"),  GT_GOL , 0x0080C, 0x0CAC00_rgb, 0x0CAC00_rgb, NGT_GOL  },
	{ ByteString("HLIF"), String("sim.gol.HLIF.name"),  String("sim.gol.HLIF.desc"), GT_HLIF, 0x0480C, 0xFF0000_rgb, 0xFF0000_rgb, NGT_HLIF },
	{ ByteString("ASIM"), String("sim.gol.ASIM.name"),  String("sim.gol.ASIM.desc"), GT_ASIM, 0x038F0, 0x0000FF_rgb, 0x0000FF_rgb, NGT_ASIM },
	{ ByteString("2X2"),  String("sim.gol.2X2.name"),   String("sim.gol.2X2.desc"),  GT_2x2 , 0x04826, 0xFFFF00_rgb, 0xFFFF00_rgb, NGT_2x2  },
	{ ByteString("DANI"), String("sim.gol.DANI.name"),  String("sim.gol.DANI.desc"), GT_DANI, 0x1C9D8, 0x00FFFF_rgb, 0x00FFFF_rgb, NGT_DANI },
	{ ByteString("AMOE"), String("sim.gol.AMOE.name"),  String("sim.gol.AMOE.desc"), GT_AMOE, 0x0A92A, 0xFF00FF_rgb, 0xFF00FF_rgb, NGT_AMOE },
	{ ByteString("MOVE"), String("sim.gol.MOVE.name"),  String("sim.gol.MOVE.desc"), GT_MOVE, 0x14834, 0xFFFFFF_rgb, 0xFFFFFF_rgb, NGT_MOVE },
	{ ByteString("PGOL"), String("sim.gol.PGOL.name"),  String("sim.gol.PGOL.desc"), GT_PGOL, 0x0A90C, 0xE05010_rgb, 0xE05010_rgb, NGT_PGOL },
	{ ByteString("DMOE"), String("sim.gol.DMOE.name"),  String("sim.gol.DMOE.desc"), GT_DMOE, 0x1E9E0, 0x500000_rgb, 0x500000_rgb, NGT_DMOE },
	{ ByteString("3-4"),  String("sim.gol.34.name"),    String("sim.gol.34.desc"),   GT_34  , 0x01818, 0x500050_rgb, 0x500050_rgb, NGT_34   },
	{ ByteString("LLIF"), String("sim.gol.LLIF.name"),  String("sim.gol.LLIF.desc"), GT_LLIF, 0x03820, 0x505050_rgb, 0x505050_rgb, NGT_LLIF },
	{ ByteString("STAN"), String("sim.gol.STAN.name"),  String("sim.gol.STAN.desc"), GT_STAN, 0x1C9EC, 0x5000FF_rgb, 0x5000FF_rgb, NGT_STAN },
	{ ByteString("SEED"), String("sim.gol.SEED.name"),  String("sim.gol.SEED.desc"), GT_SEED, 0x00400, 0xFBEC7D_rgb, 0xFBEC7D_rgb, NGT_SEED },
	{ ByteString("MAZE"), String("sim.gol.MAZE.name"),  String("sim.gol.MAZE.desc"), GT_MAZE, 0x0083E, 0xA8E4A0_rgb, 0xA8E4A0_rgb, NGT_MAZE },
	{ ByteString("COAG"), String("sim.gol.COAG.name"),  String("sim.gol.COAG.desc"), GT_COAG, 0x189EC, 0x9ACD32_rgb, 0x9ACD32_rgb, NGT_COAG },
	{ ByteString("WALL"), String("sim.gol.WALL.name"),  String("sim.gol.WALL.desc"), GT_WALL, 0x1F03C, 0x0047AB_rgb, 0x0047AB_rgb, NGT_WALL },
	{ ByteString("GNAR"), String("sim.gol.GNAR.name"),  String("sim.gol.GNAR.desc"), GT_GNAR, 0x00202, 0xE5B73B_rgb, 0xE5B73B_rgb, NGT_GNAR },
	{ ByteString("REPL"), String("sim.gol.REPL.name"),  String("sim.gol.REPL.desc"), GT_REPL, 0x0AAAA, 0x259588_rgb, 0x259588_rgb, NGT_REPL },
	{ ByteString("MYST"), String("sim.gol.MYST.name"),  String("sim.gol.MYST.desc"), GT_MYST, 0x139E1, 0x0C3C00_rgb, 0x0C3C00_rgb, NGT_MYST },
	{ ByteString("LOTE"), String("sim.gol.LOTE.name"),  String("sim.gol.LOTE.desc"), GT_LOTE, 0x48938, 0xFF0000_rgb, 0xFFFF00_rgb, NGT_LOTE },
	{ ByteString("FRG2"), String("sim.gol.FRG2.name"),  String("sim.gol.FRG2.desc"), GT_FRG2, 0x20816, 0x006432_rgb, 0x00FF5A_rgb, NGT_FRG2 },
	{ ByteString("STAR"), String("sim.gol.STAR.name"),  String("sim.gol.STAR.desc"), GT_STAR, 0x98478, 0x000040_rgb, 0x0000E6_rgb, NGT_STAR },
	{ ByteString("FROG"), String("sim.gol.FROG.name"),  String("sim.gol.FROG.desc"), GT_FROG, 0x21806, 0x006400_rgb, 0x00FF00_rgb, NGT_FROG },
	{ ByteString("BRAN"), String("sim.gol.BRAN.name"),  String("sim.gol.BRAN.desc"), GT_BRAN, 0x25440, 0xFFFF00_rgb, 0x969600_rgb, NGT_BRAN }
}};

static std::vector<wall_type> LoadWalls()
{
	return
	std::vector<wall_type>{
		{0x808080_rgb, 0x000000_rgb, 0, Renderer::WallIcon, String("sim.wall.ERASE.name"),     "DEFAULT_WL_ERASE",  String("sim.wall.ERASE.desc")},
		{0xC0C0C0_rgb, 0x101010_rgb, 0, Renderer::WallIcon, String("sim.wall.CNDTW.name"),     "DEFAULT_WL_CNDTW",  String("sim.wall.CNDTW.desc")},
		{0x808080_rgb, 0x808080_rgb, 0, Renderer::WallIcon, String("sim.wall.EWALL.name"),     "DEFAULT_WL_EWALL",  String("sim.wall.EWALL.desc")},
		{0xFF8080_rgb, 0xFF2008_rgb, 1, Renderer::WallIcon, String("sim.wall.DTECT.name"),     "DEFAULT_WL_DTECT",  String("sim.wall.DTECT.desc")},
		{0x808080_rgb, 0x000000_rgb, 0, Renderer::WallIcon, String("sim.wall.STRM.name"),      "DEFAULT_WL_STRM",   String("sim.wall.STRM.desc")},
		{0x8080FF_rgb, 0x000000_rgb, 1, Renderer::WallIcon, String("sim.wall.FAN.name"),      "DEFAULT_WL_FAN",    String("sim.wall.FAN.desc")},
		{0xC0C0C0_rgb, 0x101010_rgb, 2, Renderer::WallIcon, String("sim.wall.LIQD.name"),     "DEFAULT_WL_LIQD",   String("sim.wall.LIQD.desc")},
		{0x808080_rgb, 0x000000_rgb, 1, Renderer::WallIcon, String("sim.wall.ABSRB.name"),    "DEFAULT_WL_ABSRB",  String("sim.wall.ABSRB.desc")},
		{0x808080_rgb, 0x000000_rgb, 3, Renderer::WallIcon, String("sim.wall.WALL.name"),     "DEFAULT_WL_WALL",   String("sim.wall.WALL.desc")},
		{0x3C3C3C_rgb, 0x000000_rgb, 1, Renderer::WallIcon, String("sim.wall.AIR.name"),      "DEFAULT_WL_AIR",    String("sim.wall.AIR.desc")},
		{0x575757_rgb, 0x000000_rgb, 1, Renderer::WallIcon, String("sim.wall.POWDR.name"),     "DEFAULT_WL_POWDR",  String("sim.wall.POWDR.desc")},
		{0xFFFF22_rgb, 0x101010_rgb, 2, Renderer::WallIcon, String("sim.wall.CNDTR.name"),    "DEFAULT_WL_CNDTR",  String("sim.wall.CNDTR.desc")},
		{0x242424_rgb, 0x101010_rgb, 0, Renderer::WallIcon, String("sim.wall.EHOLE.name"),     "DEFAULT_WL_EHOLE",  String("sim.wall.EHOLE.desc")},
		{0x579777_rgb, 0x000000_rgb, 1, Renderer::WallIcon, String("sim.wall.GAS.name"),      "DEFAULT_WL_GAS",    String("sim.wall.GAS.desc")},
		{0xFFEE00_rgb, 0xAA9900_rgb, 4, Renderer::WallIcon, String("sim.wall.GRVTY.name"),    "DEFAULT_WL_GRVTY",  String("sim.wall.GRVTY.desc")},
		{0xFFAA00_rgb, 0xAA5500_rgb, 4, Renderer::WallIcon, String("sim.wall.ENRGY.name"),    "DEFAULT_WL_ENRGY",  String("sim.wall.ENRGY.desc")},
		{0xDCDCDC_rgb, 0x000000_rgb, 1, Renderer::WallIcon, String("sim.wall.NOAIR.name"),     "DEFAULT_WL_NOAIR",  String("sim.wall.NOAIR.desc")},
		{0x808080_rgb, 0x000000_rgb, 0, Renderer::WallIcon, String("sim.wall.ERASEA.name"),   "DEFAULT_WL_ERASEA", String("sim.wall.ERASEA.desc")},
		{0x800080_rgb, 0x000000_rgb, 0, Renderer::WallIcon, String("sim.wall.STASIS.name"),   "DEFAULT_WL_STASIS", String("sim.wall.STASIS.desc")},
	};
}

static std::vector<menu_section> LoadMenus()
{
	return
	std::vector<menu_section>{
		{0xE041, String("sim.menu.walls"), 0, 1},
		{0xE042, String("sim.menu.electronics"), 0, 1},
		{0xE056, String("sim.menu.powered"), 0, 1},
		{0xE019, String("sim.menu.sensors"), 0, 1},
		{0xE062, String("sim.menu.force"), 0, 1},
		{0xE043, String("sim.menu.explosives"), 0, 1},
		{0xE045, String("sim.menu.gases"), 0, 1},
		{0xE044, String("sim.menu.liquids"), 0, 1},
		{0xE050, String("sim.menu.powders"), 0, 1},
		{0xE051, String("sim.menu.solids"), 0, 1},
		{0xE046, String("sim.menu.radioactive"), 0, 1},
		{0xE04C, String("sim.menu.special"), 0, 1},
		{0xE052, String("sim.menu.gol"), 0, 1},
		{0xE057, String("sim.menu.tools"), 0, 1},
		{0xE067, String("sim.menu.favorites"), 0, 1},
		{0xE064, String("sim.menu.deco"), 0, 1},
	};
}

void SimulationData::init_can_move()
{
	int movingType, destinationType;
	// can_move[moving type][type at destination]
	//  0 = No move/Bounce
	//  1 = Swap
	//  2 = Both particles occupy the same space.
	//  3 = Varies, go run some extra checks

	//particles that don't exist shouldn't move...
	for (destinationType = 0; destinationType < PT_NUM; destinationType++)
		can_move[0][destinationType] = 0;

	//initialize everything else to swapping by default
	for (movingType = 1; movingType < PT_NUM; movingType++)
		for (destinationType = 0; destinationType < PT_NUM; destinationType++)
			can_move[movingType][destinationType] = 1;

	//photons go through everything by default
	for (destinationType = 1; destinationType < PT_NUM; destinationType++)
		can_move[PT_PHOT][destinationType] = 2;

	for (movingType = 1; movingType < PT_NUM; movingType++)
	{
		for (destinationType = 1; destinationType < PT_NUM; destinationType++)
		{
			//weight check, also prevents particles of same type displacing each other
			if (elements[movingType].Weight <= elements[destinationType].Weight || destinationType == PT_GEL)
				can_move[movingType][destinationType] = 0;

			//other checks for NEUT and energy particles
			if (movingType == PT_NEUT && (elements[destinationType].Properties&PROP_NEUTPASS))
				can_move[movingType][destinationType] = 2;
			if (movingType == PT_NEUT && (elements[destinationType].Properties&PROP_NEUTABSORB))
				can_move[movingType][destinationType] = 1;
			if (movingType == PT_NEUT && (elements[destinationType].Properties&PROP_NEUTPENETRATE))
				can_move[movingType][destinationType] = 1;
			if (destinationType == PT_NEUT && (elements[movingType].Properties&PROP_NEUTPENETRATE))
				can_move[movingType][destinationType] = 0;
			if ((elements[movingType].Properties&TYPE_ENERGY) && (elements[destinationType].Properties&TYPE_ENERGY))
				can_move[movingType][destinationType] = 2;
		}
	}
	for (destinationType = 0; destinationType < PT_NUM; destinationType++)
	{
		//set what stickmen can move through
		int stkm_move = 0;
		if (elements[destinationType].Properties & (TYPE_LIQUID | TYPE_GAS))
			stkm_move = 2;
		if (!destinationType || destinationType == PT_PRTO || destinationType == PT_SPAWN || destinationType == PT_SPAWN2)
			stkm_move = 2;
		can_move[PT_STKM][destinationType] = stkm_move;
		can_move[PT_STKM2][destinationType] = stkm_move;
		can_move[PT_FIGH][destinationType] = stkm_move;

		//spark shouldn't move
		can_move[PT_SPRK][destinationType] = 0;
	}
	for (movingType = 1; movingType < PT_NUM; movingType++)
	{
		//everything "swaps" with VACU and BHOL to make them eat things
		can_move[movingType][PT_BHOL] = 1;
		can_move[movingType][PT_NBHL] = 1;
		//nothing goes through stickmen
		can_move[movingType][PT_STKM] = 0;
		can_move[movingType][PT_STKM2] = 0;
		can_move[movingType][PT_FIGH] = 0;
		//INVS behaviour varies with pressure
		can_move[movingType][PT_INVIS] = 3;
		//stop CNCT from being displaced by other particles
		can_move[movingType][PT_CNCT] = 0;
		//VOID and PVOD behaviour varies with powered state and ctype
		can_move[movingType][PT_PVOD] = 3;
		can_move[movingType][PT_VOID] = 3;
		//nothing moves through EMBR (not sure why, but it's killed when it touches anything)
		can_move[movingType][PT_EMBR] = 0;
		can_move[PT_EMBR][movingType] = 0;
		//Energy particles move through VIBR and BVBR, so it can absorb them
		if (elements[movingType].Properties & TYPE_ENERGY)
		{
			can_move[movingType][PT_VIBR] = 1;
			can_move[movingType][PT_BVBR] = 1;
		}

		//SAWD cannot be displaced by other powders
		if (elements[movingType].Properties & TYPE_PART)
			can_move[movingType][PT_SAWD] = 0;
	}

	for (destinationType = 0; destinationType < PT_NUM; destinationType++)
	{
		//a list of lots of things PHOT can move through
		if (elements[destinationType].Properties & PROP_PHOTPASS)
			can_move[PT_PHOT][destinationType] = 2;

		//Things PROT and GRVT cannot move through
		if (destinationType != PT_DMND && destinationType != PT_INSL && destinationType != PT_VOID && destinationType != PT_PVOD && destinationType != PT_VIBR && destinationType != PT_BVBR && destinationType != PT_PRTI && destinationType != PT_PRTO)
		{
			can_move[PT_PROT][destinationType] = 2;
			can_move[PT_GRVT][destinationType] = 2;
		}
	}

	//other special cases that weren't covered above
	can_move[PT_DEST][PT_DMND] = 0;
	can_move[PT_DEST][PT_CLNE] = 0;
	can_move[PT_DEST][PT_PCLN] = 0;
	can_move[PT_DEST][PT_BCLN] = 0;
	can_move[PT_DEST][PT_PBCN] = 0;
	can_move[PT_DEST][PT_ROCK] = 0;

	can_move[PT_NEUT][PT_INVIS] = 2;
	can_move[PT_ELEC][PT_LCRY] = 2;
	can_move[PT_ELEC][PT_EXOT] = 2;
	can_move[PT_ELEC][PT_GLOW] = 2;
	can_move[PT_PHOT][PT_LCRY] = 3; //varies according to LCRY life
	can_move[PT_PHOT][PT_GPMP] = 3;

	can_move[PT_PHOT][PT_BIZR] = 2;
	can_move[PT_ELEC][PT_BIZR] = 2;
	can_move[PT_PHOT][PT_BIZRG] = 2;
	can_move[PT_ELEC][PT_BIZRG] = 2;
	can_move[PT_PHOT][PT_BIZRS] = 2;
	can_move[PT_ELEC][PT_BIZRS] = 2;
	can_move[PT_BIZR][PT_FILT] = 2;
	can_move[PT_BIZRG][PT_FILT] = 2;

	can_move[PT_ANAR][PT_WHOL] = 1; //WHOL eats ANAR
	can_move[PT_ANAR][PT_NWHL] = 1;
	can_move[PT_ELEC][PT_DEUT] = 1;
	can_move[PT_THDR][PT_THDR] = 2;
	can_move[PT_EMBR][PT_EMBR] = 2;
	can_move[PT_TRON][PT_SWCH] = 3;
	can_move[PT_ELEC][PT_RSST] = 2;
	can_move[PT_ELEC][PT_RSSS] = 2;
}

const CustomGOLData *SimulationData::GetCustomGOLByRule(int rule) const
{
	// * Binary search. customGol is already sorted, see SetCustomGOL.
	auto it = std::lower_bound(customGol.begin(), customGol.end(), rule, [](const CustomGOLData &item, int rule) {
		return item.rule < rule;
	});
	if (it != customGol.end() && !(rule < it->rule))
	{
		return &*it;
	}
	return nullptr;
}

void SimulationData::SetCustomGOL(std::vector<CustomGOLData> newCustomGol)
{
	std::sort(newCustomGol.begin(), newCustomGol.end());
	customGol = newCustomGol;
}

String SimulationData::ElementResolve(int type, int ctype) const
{
	if (type == PT_LIFE)
	{
		if (ctype >= 0 && ctype < NGOL)
		{
			return Localization::Ref().Tr(builtinGol[ctype].nameKey.ToUtf8().c_str());
		}
		auto *cgol = GetCustomGOLByRule(ctype);
		if (cgol)
		{
			return cgol->nameString;
		}
		return SerialiseGOLRule(ctype);
	}
	else if (type >= 0 && type < PT_NUM)
		return elements[type].Name;
	return Localization::Ref().Tr("sim.empty");
}

String SimulationData::BasicParticleInfo(Particle const &sample_part) const
{
	StringBuilder sampleInfo;
	int type = sample_part.type;
	int ctype = sample_part.ctype;
	int storedCtype = sample_part.tmp4;
	if (type == PT_LAVA && IsElement(ctype))
	{
		sampleInfo << Localization::Ref().Tr("sim.molten_prefix") << ElementResolve(ctype, -1);
	}
	else if ((type == PT_PIPE || type == PT_PPIP) && IsElement(ctype))
	{
		if (ctype == PT_LAVA && IsElement(storedCtype))
		{
			sampleInfo << ElementResolve(type, -1) << Localization::Ref().Tr("sim.with_molten") << ElementResolve(storedCtype, -1);
		}
		else
		{
			sampleInfo << ElementResolve(type, -1) << " with " << ElementResolve(ctype, storedCtype);
		}
	}
	else
	{
		sampleInfo << ElementResolve(type, ctype);
	}
	return sampleInfo.Build();
}

int SimulationData::GetParticleType(ByteString type) const
{
	type = type.ToUpper();

	// alternative names for some elements
	if (byteStringEqualsLiteral(type, "C4"))
	{
		return PT_PLEX;
	}
	else if (byteStringEqualsLiteral(type, "C5"))
	{
		return PT_C5;
	}
	else if (byteStringEqualsLiteral(type, "NONE"))
	{
		return PT_NONE;
	}
	for (int i = 1; i < PT_NUM; i++)
	{
		if (elements[i].Name.size() && elements[i].Enabled && type == elements[i].Name.ToUtf8().ToUpper())
		{
			return i;
		}
	}
	return -1;
}

SimulationData::SimulationData()
{
	msections = LoadMenus();
	wtypes = LoadWalls();
	elements = GetElements();
	init_can_move();
}

bool SimulationData::IsHeatInsulator(const Particle &p) const
{
	return elements[p.type].HeatConduct == 0 || (p.type == PT_HSWC && p.life != 10) || ((p.type == PT_PIPE || p.type == PT_PPIP) && (p.tmp & PFLAG_CAN_CONDUCT) == 0);
}

float SimulationData::HeatCapacityOf(const Particle &p) const
{
	if ((p.type == PT_PIPE || p.type == PT_PPIP) && IsElement(p.ctype))
		return elements[p.type].HeatCapacity + elements[p.ctype].HeatCapacity;
	else
		return elements[p.type].HeatCapacity;
}
