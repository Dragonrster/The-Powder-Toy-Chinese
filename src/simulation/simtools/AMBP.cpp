#include "simulation/ToolCommon.h"
#include "common/Localization.h"

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_AMBP()
{
	Identifier = "DEFAULT_TOOL_AMBP";
	Name = "AMBP";
	Colour = 0xFFDD00_rgb;
	Description = Localization::Ref().Tr("sim.tool.DEFAULT_TOOL_AMBP");
	Perform = &perform;
}

static int perform(SimTool *tool, Simulation *sim, Particle *cpart, int x, int y, int brushX, int brushY, float strength)
{
	if (!sim->aheat_enable)
	{
		return 0;
	}

	sim->hv[y / CELL][x / CELL] += strength * 2.0f;
	if (sim->hv[y / CELL][x / CELL] > MAX_TEMP) sim->hv[y / CELL][x / CELL] = MAX_TEMP;
	if (sim->hv[y / CELL][x / CELL] < MIN_TEMP) sim->hv[y / CELL][x / CELL] = MIN_TEMP;
	return 1;
}
