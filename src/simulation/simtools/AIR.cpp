#include "simulation/ToolCommon.h"
#include "simulation/Air.h"

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_AIR()
{
	Identifier = "DEFAULT_TOOL_AIR";
	Name = "AIR";
	Colour = 0xFFFFFF_rgb;
	Description = ByteString("氣壓工具,增加目標區域內的氣壓").FromUtf8();
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
