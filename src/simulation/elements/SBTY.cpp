#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_SBTY()
{
	Identifier = "DEFAULT_PT_SBTY";
	Name = "SBTY";
	Colour = 0x256229_rgb;
	MenuVisible = 1;
	MenuSection = SC_ELEC;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 251;
	Description = ByteString("超级电池,产生无限的次级电脉冲").FromUtf8();

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 2273.0f;
	HighTemperatureTransition = PT_PLSM;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS)
{
	auto &sd = SimulationData::CRef();
	auto &elements = sd.elements;
	for (auto rx = -2; rx <= 2; rx++)
	{
		for (auto ry = -2; ry <= 2; ry++)
		{
			if ((rx || ry) && abs(rx) + abs(ry) < 4)
			{
				auto r = pmap[y + ry][x + rx];
				if (!r)
					continue;
				auto rt = TYP(r);
				auto pavg = sim->parts_avg(i, ID(r), PT_INSL);
				if (pavg != PT_INSL && pavg != PT_RSSS)
				{
					if (rt != PT_SPRK)
					{
						if ((elements[rt].Properties & PROP_CONDUCTS) && !(rt == PT_WATR || rt == PT_SLTW || rt == PT_NTCT || rt == PT_PTCT || rt == PT_INWR))
						{
							parts[ID(r)].life = 4;
							parts[ID(r)].ctype = rt;
							sim->part_change_type(ID(r), x + rx, y + ry, PT_SPRK);
						}
					}
					else // 如果是 PT_SPRK 类型的粒子
					{
						parts[ID(r)].life = 4; // 设置生命为4
					}
				}
			}
		}
	}
	return 0;
}
