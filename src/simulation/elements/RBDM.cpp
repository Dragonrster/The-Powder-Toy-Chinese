#include "simulation/ElementCommon.h"

void Element::Element_RBDM()
{
	Identifier = "DEFAULT_PT_RBDM";
	Name = "RBDM";
	Colour = 0xCCCCCC_rgb;
	MenuVisible = 1;
	MenuSection = SC_EXPLOSIVE;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 1000;
	Explosive = 1;
	Meltable = 50;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 240;
	Description = ByteString("固态铷,低熔点,可以导电,遇水爆炸").FromUtf8();

	Properties = TYPE_SOLID|PROP_CONDUCTS|PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 312.0f;
	HighTemperatureTransition = PT_LRBD;
}
