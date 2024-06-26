#include "simulation/ElementCommon.h"

void Element::Element_LO2()
{
	Identifier = "DEFAULT_PT_LO2";
	Name = "LOXY";
	Colour = 0x80A0EF_rgb;
	MenuVisible = 1;
	MenuSection = SC_LIQUID;
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 2;

	Flammable = 5000;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 30;

	DefaultProperties.temp = 80.0f;
	HeatConduct = 70;
	Description = ByteString("液氧,点燃时产生等离子体,升温时转变成氧气").FromUtf8();

	Properties = TYPE_LIQUID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 90.1f;
	HighTemperatureTransition = PT_O2;
}
