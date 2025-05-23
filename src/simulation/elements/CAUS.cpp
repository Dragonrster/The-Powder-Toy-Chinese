#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_CAUS()
{
	Identifier = "DEFAULT_PT_CAUS";
	Name = "CAUS";
	Colour = 0x80FFA0_rgb;
	MenuVisible = 1;
	MenuSection = SC_GAS;
	Enabled = 1;

	Advection = 2.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.99f;
	Loss = 0.30f;
	Collision = -0.1f;
	Gravity = 0.0f;
	Diffusion = 1.50f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 1;

	HeatConduct = 70;
	Description = ByteString("酸性气体,性质和酸(ACID)相似").FromUtf8();

	Properties = TYPE_GAS|PROP_DEADLY;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	DefaultProperties.life = 75;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS)
{
	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;
	bool converted = false;
	for (int rx = -2; rx <= 2; rx++)
	{
		for (int ry = -2; ry <= 2; ry++)
		{
			if (rx || ry)
			{
				int r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if (TYP(r) == PT_GAS)
				{
					if (sim->pv[(y+ry)/CELL][(x+rx)/CELL] > 3)
					{
						sim->part_change_type(ID(r), x+rx, y+ry, PT_RFRG);
						sim->part_change_type(i, x, y, PT_RFRG);
						converted = true;
					}
				}
				else if (TYP(r) != PT_ACID && TYP(r) != PT_CAUS && TYP(r) != PT_RFRG && TYP(r) != PT_RFGL)
				{
					if ((TYP(r) != PT_CLNE && TYP(r) != PT_PCLN && ((TYP(r) != PT_FOG && TYP(r) != PT_RIME) || parts[ID(r)].tmp <= 5) && sim->rng.chance(elements[TYP(r)].Hardness, 1000)) && parts[i].life > 50)
					{
						// GLAS protects stuff from acid
						if (sim->parts_avg(i, ID(r),PT_GLAS) != PT_GLAS)
						{
							float newtemp = ((60.0f - (float)elements[TYP(r)].Hardness)) * 7.0f;
							if (newtemp < 0)
								newtemp = 0;
							parts[i].temp += newtemp;
							parts[i].life--;
							sim->kill_part(ID(r));
						}
					}
					else if (parts[i].life <= 50)
					{
						sim->kill_part(i);
						return 1;
					}
				}
			}
		}
	}
	return converted;
}
