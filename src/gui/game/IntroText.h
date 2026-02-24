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
	ByteStringBuilder sb;  
	sb << "\bl\bU" << APPNAME << "\bU - Version " << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1] << " - https://powdertoy.co.uk, irc.libera.chat #powder, https://tpt.io/discord\n"  
	      "\n"  
	      "\n"  
	      "\bg" << Localization::Ref().Tr("intro.control.copy_paste_cut").ToUtf8() << "\n"  
	      "\bg" << Localization::Ref().Tr("intro.material.hover").ToUtf8() << "\n"  
	      "\bg" << Localization::Ref().Tr("intro.material.pick").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.draw.freeform").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.draw.straight").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.draw.rectangles").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.draw.flood_fill").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.tool.size").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.tool.sample").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.tool.undo").ToUtf8() << "\n"  
	      "\n\bo" << Localization::Ref().Tr("intro.zoom.tool").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.sim.pause").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.save.stamps").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.screenshot").ToUtf8() << "\n"  
	      << Localization::Ref().Tr("intro.hud.debug").ToUtf8() << "\n"  
	      "\n";  
	if constexpr (BETA)  
	{  
		sb << "\br" << Localization::Ref().Tr("intro.beta.warning").ToUtf8() << "\n"  
		      "\br" << Localization::Ref().Tr("intro.beta.publish").ToUtf8() << "\n";  
	}  
	else  
	{  
		sb << "\bg" << Localization::Ref().Tr("intro.online.register").ToUtf8() << " \br" << SERVER << "/Register.html\n";  
	}  
	sb << "\n\bt" << VersionInfo();  
	return sb.Build();  
}