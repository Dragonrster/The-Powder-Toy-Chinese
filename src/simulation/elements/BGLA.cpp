#include "simulation/ElementCommon.h"

void Element::Element_BGLA()
{
	Identifier = "DEFAULT_PT_BGLA";
	Name = "BGLA";
	Colour = 0x606060_rgb;
	MenuVisible = 1;
	MenuSection = SC_POWDERS;
	Enabled = 1;

	Advection = 0.4f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 5;
	Hardness = 0;

	Weight = 90;

	HeatConduct = 150;
	Description = ByteString("碎玻璃,玻璃在压力下破碎,熔化后能重新变回玻璃").FromUtf8();

	Properties = TYPE_PART | PROP_NEUTPASS | PROP_PHOTPASS | PROP_HOT_GLOW | PROP_DEADLY;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 1973.0f;
	HighTemperatureTransition = PT_LAVA;
}
