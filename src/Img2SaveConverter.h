#pragma once
#include "graphics/Pixel.h"
#include "simulation/Particle.h"
#include "client/GameSave.h"
#include "common/String.h"
#include <vector>
#include <memory>

struct ColorPaletteEntry
{
	int elementId;
	RGB color;
};

class Img2SaveConverter
{
public:
	static std::vector<ColorPaletteEntry> BuildPalette();
	static int FindClosestElement(RGB pixel, const std::vector<ColorPaletteEntry> &palette);
	static bool LoadImage(const std::vector<char> &fileData,
	                      std::vector<pixel_rgba> &outPixels,
	                      int &outWidth, int &outHeight,
	                      String &outError);
	static std::vector<pixel_rgba> ResizeImage(const std::vector<pixel_rgba> &pixels,
	                                           int srcWidth, int srcHeight,
	                                           int dstWidth, int dstHeight);
	static std::unique_ptr<GameSave> GenerateSave(const std::vector<pixel_rgba> &pixels,
	                                              int width, int height,
	                                              const std::vector<ColorPaletteEntry> &palette);
	static std::unique_ptr<GameSave> Convert(const std::vector<char> &fileData,
	                                         String &outError);
};
