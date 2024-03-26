#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_SIBO()
{
	Identifier = "DEFAULT_PT_SIBO";
	Name = "SIBO";
	Colour = 0x9cdcfe_rgb;
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = 0.7f;
	AirDrag = 0.36f * CFDS;
	AirLoss = 0.96f;
	Loss = 0.80f;
	Collision = 0.1f;
	Gravity = 0.12f;
	Diffusion = 0.00f;
	HotAir = -0.001f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 86;

	HeatConduct = 70;
	Description = ByteString("奇点炸弹,会产生破坏其它物质,本质上是粉末状的黑洞").FromUtf8();

	Properties = TYPE_PART | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
	Create = &create;
}

static int update(UPDATE_FUNC_ARGS)
{
	switch (sim->rng.gen() % 3)
	{
	case 0:
		nb = sim->create_part(-3, x, y, PT_PHOT);
		break;
	case 1:
		nb = sim->create_part(-3, x, y, PT_NEUT);
		break;
	case 2:
		nb = sim->create_part(-3, x, y, PT_ELEC);
		break;
	}
	return 0;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	sim->parts[i].life = sim->rng.between(60, 109);
}
