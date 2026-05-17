#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Img2SaveConverter.h"
#include "SimulationConfig.h"
#include "simulation/SimulationData.h"
#include "simulation/ElementDefs.h"
#include <algorithm>
#include <cmath>

std::vector<ColorPaletteEntry> Img2SaveConverter::BuildPalette()
{
	std::vector<ColorPaletteEntry> palette;
	auto &elements = SimulationData::CRef().elements;
	for (int i = 1; i < PT_NUM; i++)
	{
		if (elements[i].Enabled)
		{
			palette.push_back({ i, elements[i].Colour });
		}
	}
	return palette;
}

int Img2SaveConverter::FindClosestElement(RGB pixel, const std::vector<ColorPaletteEntry> &palette)
{
	int bestElement = palette[0].elementId;
	int bestDist = 0x7FFFFFFF;
	for (auto &entry : palette)
	{
		int dr = (int)pixel.Red   - (int)entry.color.Red;
		int dg = (int)pixel.Green - (int)entry.color.Green;
		int db = (int)pixel.Blue  - (int)entry.color.Blue;
		int dist = dr * dr + dg * dg + db * db;
		if (dist < bestDist)
		{
			bestDist = dist;
			bestElement = entry.elementId;
		}
	}
	return bestElement;
}

bool Img2SaveConverter::LoadImage(const std::vector<char> &fileData,
                                  std::vector<pixel_rgba> &outPixels,
                                  int &outWidth, int &outHeight,
                                  String &outError)
{
	int w, h, channels;
	auto *data = stbi_load_from_memory(
		reinterpret_cast<const stbi_uc *>(fileData.data()),
		static_cast<int>(fileData.size()),
		&w, &h, &channels, 4);
	if (!data)
	{
		outError = String::Build("Failed to decode image: ", ByteString(stbi_failure_reason()).FromUtf8());
		return false;
	}
	if (w <= 0 || h <= 0)
	{
		stbi_image_free(data);
		outError = "Image has zero dimensions";
		return false;
	}

	outWidth = w;
	outHeight = h;
	outPixels.resize(w * h);
	for (int i = 0; i < w * h; i++)
	{
		auto r = data[i * 4 + 0];
		auto g = data[i * 4 + 1];
		auto b = data[i * 4 + 2];
		auto a = data[i * 4 + 3];
		outPixels[i] = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b) | (uint32_t(a) << 24);
	}
	stbi_image_free(data);
	return true;
}

std::vector<pixel_rgba> Img2SaveConverter::ResizeImage(const std::vector<pixel_rgba> &pixels,
                                                       int srcWidth, int srcHeight,
                                                       int dstWidth, int dstHeight)
{
	if (srcWidth == dstWidth && srcHeight == dstHeight)
		return pixels;

	std::vector<pixel_rgba> result(dstWidth * dstHeight);

	if (dstWidth < srcWidth && dstHeight < srcHeight)
	{
		// Box-filtered downscaling
		double xRatio = (double)srcWidth / dstWidth;
		double yRatio = (double)srcHeight / dstHeight;
		for (int dy = 0; dy < dstHeight; dy++)
		{
			int yStart = (int)(dy * yRatio);
			int yEnd = (int)((dy + 1) * yRatio);
			for (int dx = 0; dx < dstWidth; dx++)
			{
				int xStart = (int)(dx * xRatio);
				int xEnd = (int)((dx + 1) * xRatio);
				int rSum = 0, gSum = 0, bSum = 0, aSum = 0, count = 0;
				for (int sy = yStart; sy < yEnd; sy++)
				{
					for (int sx = xStart; sx < xEnd; sx++)
					{
						auto px = pixels[sy * srcWidth + sx];
						rSum += (px >> 16) & 0xFF;
						gSum += (px >>  8) & 0xFF;
						bSum +=  px        & 0xFF;
						aSum += (px >> 24) & 0xFF;
						count++;
					}
				}
				if (count > 0)
				{
					auto r = rSum / count;
					auto g = gSum / count;
					auto b = bSum / count;
					auto a = aSum / count;
					result[dy * dstWidth + dx] = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b) | (uint32_t(a) << 24);
				}
			}
		}
	}
	else
	{
		// Nearest-neighbor upscaling for one or both dimensions
		for (int dy = 0; dy < dstHeight; dy++)
		{
			int sy = dy * srcHeight / dstHeight;
			for (int dx = 0; dx < dstWidth; dx++)
			{
				int sx = dx * srcWidth / dstWidth;
				result[dy * dstWidth + dx] = pixels[sy * srcWidth + sx];
			}
		}
	}

	return result;
}

std::unique_ptr<GameSave> Img2SaveConverter::GenerateSave(const std::vector<pixel_rgba> &pixels,
                                                          int width, int height,
                                                          const std::vector<ColorPaletteEntry> &palette)
{
	int blockW = (width  + CELL - 1) / CELL;
	int blockH = (height + CELL - 1) / CELL;
	auto gameSave = std::make_unique<GameSave>(Vec2(blockW, blockH));

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			auto px = pixels[y * width + x];
			uint8_t alpha = (px >> 24) & 0xFF;
			if (alpha < 128)
				continue;

			uint8_t r = (px >> 16) & 0xFF;
			uint8_t g = (px >>  8) & 0xFF;
			uint8_t b =  px        & 0xFF;

			int elementType = FindClosestElement(RGB(r, g, b), palette);

			Particle part{};
			part.type = elementType;
			part.x = (float)x + 0.5f;
			part.y = (float)y + 0.5f;
			part.temp = R_TEMP + 273.15f;
			*gameSave << part;
		}
	}

	return gameSave;
}

std::unique_ptr<GameSave> Img2SaveConverter::Convert(const std::vector<char> &fileData,
                                                     String &outError)
{
	int imgWidth, imgHeight;
	std::vector<pixel_rgba> imgPixels;
	if (!LoadImage(fileData, imgPixels, imgWidth, imgHeight, outError))
		return nullptr;

	// Fit image within simulation grid maintaining aspect ratio
	int dstWidth = imgWidth;
	int dstHeight = imgHeight;
	if (dstWidth > XRES)
	{
		dstHeight = dstHeight * XRES / dstWidth;
		dstWidth = XRES;
	}
	if (dstHeight > YRES)
	{
		dstWidth = dstWidth * YRES / dstHeight;
		dstHeight = YRES;
	}
	if (dstWidth < 1) dstWidth = 1;
	if (dstHeight < 1) dstHeight = 1;

	auto resizedPixels = ResizeImage(imgPixels, imgWidth, imgHeight, dstWidth, dstHeight);

	auto palette = BuildPalette();
	if (palette.empty())
	{
		outError = "No enabled elements for conversion";
		return nullptr;
	}

	return GenerateSave(resizedPixels, dstWidth, dstHeight, palette);
}
