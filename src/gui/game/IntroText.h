#pragma once
#include "Config.h"
#include "SimulationConfig.h"
#include "common/String.h"
#include "common/Localization.h"

inline ByteString VersionInfo()
{
	ByteStringBuilder sb;
	sb << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1];
	if constexpr (!SNAPSHOT)
	{
		sb << "." << APP_VERSION.build;
	}
	sb << " " << IDENT;
	if constexpr (MOD)
	{
		sb << " MOD " << MOD_ID << " UPSTREAM " << UPSTREAM_VERSION.build;
	}
	if constexpr (SNAPSHOT)
	{
		sb << " SNAPSHOT " << APP_VERSION.build;
	}
	if constexpr (LUACONSOLE)
	{
		sb << " LUACONSOLE";
	}
	if constexpr (NOHTTP)
	{
		sb << " NOHTTP";
	}
	else if constexpr (ENFORCE_HTTPS)
	{
		sb << " HTTPS";
	}
	if constexpr (DEBUG)
	{
		sb << " DEBUG";
	}
	return sb.Build();
}

inline ByteString IntroText()
{
	auto tr = [](const char *key) { return Localization::Ref().Tr(key).ToUtf8(); };
	ByteStringBuilder sb;
	sb << tr("intro.title_prefix") << APPNAME << tr("intro.title_after_name") << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1] << tr("intro.title_after_version")
	      << tr("intro.control.copy_paste_cut")
	      << tr("intro.material.hover")
	      << tr("intro.material.pick")
	      << tr("intro.draw.freeform")
	      << tr("intro.draw.straight")
	      << tr("intro.draw.rectangles")
	      << tr("intro.draw.flood_fill")
	      << tr("intro.tool.size")
	      << tr("intro.tool.sample")
	      << tr("intro.tool.undo")
	      << tr("intro.zoom.tool")
	      << tr("intro.sim.pause")
	      << tr("intro.save.stamps")
	      << tr("intro.screenshot")
	      << tr("intro.hud.debug")
	      << "\n";
	if constexpr (BETA)
	{
		sb << tr("intro.beta.warning")
		   << tr("intro.beta.publish");
	}
	else
	{
		sb << tr("intro.online.register") << "\br" << SERVER << "/Register.html\n";
	}
	sb << tr("intro.version_prefix") << VersionInfo();
	return sb.Build();
}