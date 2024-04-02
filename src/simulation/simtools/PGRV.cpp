#include "simulation/ToolCommon.h"

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_PGRV()
{
	Identifier = "DEFAULT_TOOL_PGRV";
	Name = "PGRV";
	Colour = 0xCCCCFF_rgb;
	Description = ByteString("正引力工具,制造一个正引力源,一段时间后消失").FromUtf8();
	Perform = &perform;
}

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength)
{
	sim->gravmap[((y/CELL)*XCELLS)+(x/CELL)] = strength*5.0f;
	return 1;
}
