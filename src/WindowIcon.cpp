#include "Format.h"
#include "graphics/VideoBuffer.h"
#include "WindowIcon.h"

#include "icon_exe_png.h"

void WindowIcon(SDL_Window *window)
{
	if (auto image = format::PixelsFromPNG(icon_exe_png.AsCharSpan()))
	{
		// SDL3: SDL_CreateSurfaceFrom takes (w, h, format, pixels, pitch);
		// masks are no longer passed explicitly. Our pixel data is ARGB8888.
		SDL_Surface *icon = SDL_CreateSurfaceFrom(image->Size().X, image->Size().Y, SDL_PIXELFORMAT_ARGB8888, image->data(), image->Size().X * sizeof(pixel));
		SDL_SetWindowIcon(window, icon);
		SDL_DestroySurface(icon);
	}
}
