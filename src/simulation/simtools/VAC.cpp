#include "simulation/ToolCommon.h"
#include "simulation/Air.h"

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_VAC()
{
	Identifier = "DEFAULT_TOOL_VAC";
	Name = "VAC";
	Colour = 0x303030_rgb;
	Description = ByteString("真空工具,降低目標區域內的氣壓").FromUtf8();
	Perform = &perform;
}

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength)
{
	sim->pv[y/CELL][x/CELL] -= strength*0.05f;

	if (sim->pv[y/CELL][x/CELL] > MAX_PRESSURE)
		sim->pv[y/CELL][x/CELL] = MAX_PRESSURE;
	else if (sim->pv[y/CELL][x/CELL] < MIN_PRESSURE)
		sim->pv[y/CELL][x/CELL] = MIN_PRESSURE;
	return 1;
}
