#pragma once
// SDL3 removed SDL_syswm.h and the SDL_SYSWM_* subsystem enum entirely.
// The clipboard code used SDL_SYSWM_TYPE to decide which native clipboard
// backend to use. We reimplement an equivalent enum here and detect the
// active windowing subsystem via SDL_GetCurrentVideoDriver(), which returns
// a driver name string such as "x11", "wayland", "windows" or "cocoa".
#include <SDL3/SDL.h>
#include "common/String.h"

namespace Clipboard
{
	enum WindowSubsystem
	{
		SDL_SYSWM_UNKNOWN = 0,
		SDL_SYSWM_WINDOWS,
		SDL_SYSWM_X11,
		SDL_SYSWM_WAYLAND,
		SDL_SYSWM_COCOA,
	};
	using SDL_SYSWM_TYPE = WindowSubsystem;

	inline int DetectWindowSubsystem()
	{
		const char *driver = SDL_GetCurrentVideoDriver();
		if (!driver)
		{
			return SDL_SYSWM_UNKNOWN;
		}
		ByteString name = driver;
		if (name == "x11")
		{
			return SDL_SYSWM_X11;
		}
		if (name == "wayland")
		{
			return SDL_SYSWM_WAYLAND;
		}
		if (name == "windows")
		{
			return SDL_SYSWM_WINDOWS;
		}
		if (name == "cocoa")
		{
			return SDL_SYSWM_COCOA;
		}
		return SDL_SYSWM_UNKNOWN;
	}
}
