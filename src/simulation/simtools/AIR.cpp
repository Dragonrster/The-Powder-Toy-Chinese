#include "simulation/ToolCommon.h"
#include "simulation/Air.h"
#include "common/Localization.h"

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_AIR()
{
	Identifier = "DEFAULT_TOOL_AIR";
	Name = "AIR";
	Colour = 0xFFFFFF_rgb;
	Description = Localization::Ref().Tr("sim.tool.DEFAULT_TOOL_AIR");
	Perform = &perform;
}

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength)
{
	sim->pv[y/CELL][x/CELL] += strength*0.05f;

	if (sim->pv[y/CELL][x/CELL] > MAX_PRESSURE)
		sim->pv[y/CELL][x/CELL] = MAX_PRESSURE;
	else if (sim->pv[y/CELL][x/CELL] < MIN_PRESSURE)
		sim->pv[y/CELL][x/CELL] = MIN_PRESSURE;
	return 1;
}
