#include "simulation/ToolCommon.h"

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushYy, float strength);

void SimTool::Tool_NGRV()
{
	Identifier = "DEFAULT_TOOL_NGRV";
	Name = "NGRV";
	Colour = 0xAACCFF_rgb;
	Description = ByteString("負引力工具,製造一個負引力源,一段時間後消失").FromUtf8();
	Perform = &perform;
}

static int perform(SimTool *tool, Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushYy, float strength)
{
	sim->gravIn.mass[Vec2{ x, y } / CELL] = strength * -5.0f;
	return 1;
}
