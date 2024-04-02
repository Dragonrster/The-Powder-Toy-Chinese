#include "simulation/ElementCommon.h"

void Element::Element_NITR()
{
	Identifier = "DEFAULT_PT_NITR";
	Name = "NITR";
	Colour = 0x20E010_rgb;
	MenuVisible = 1;
	MenuSection = SC_EXPLOSIVE;
	Enabled = 1;

	Advection = 0.5f;
	AirDrag = 0.02f * CFDS;
	AirLoss = 0.92f;
	Loss = 0.97f;
	Collision = 0.0f;
	Gravity = 0.2f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 2;

	Flammable = 1000;
	Explosive = 2;
	Meltable = 0;
	Hardness = 3;
	PhotonReflectWavelengths = 0x0007C000;

	Weight = 23;

	HeatConduct = 50;
	Description = ByteString("硝化甘油,压力下,电脉冲,明火都可以引起爆炸").FromUtf8();

	Properties = TYPE_LIQUID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 673.0f;
	HighTemperatureTransition = PT_FIRE;
}
