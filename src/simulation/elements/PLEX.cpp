#include "simulation/ElementCommon.h"

void Element::Element_PLEX()
{
	Identifier = "DEFAULT_PT_PLEX";
	Name = "C-4";
	Colour = 0xD080E0_rgb;
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
	Explosive = 2;
	Meltable = 50;
	Hardness = 1;
	PhotonReflectWavelengths = 0x1F00003E;

	Weight = 100;

	HeatConduct = 88;
	Description = ByteString("C-4塑胶炸弹,压力敏感型炸药,暴露在高压下,电脉冲或者达到爆炸点都可以引发爆炸").FromUtf8();

	Properties = TYPE_SOLID | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 673.0f;
	HighTemperatureTransition = PT_FIRE;
}
