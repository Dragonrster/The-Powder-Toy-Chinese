#include "graphics/FTText.h"
#include "graphics/Pixel.h"
#include "graphics.h"
#include "common/Format.h"
#define INCLUDE_FONTDATA
#include "font.h"
#include <cstring>
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library  ft_library = nullptr;
static FT_Face     ft_face = nullptr;
static int         ft_size = 12;
static bool        ft_ready = false;

// --- UTF-8 decoder ---

static unsigned utf8_decode(const char **s)
{
	const unsigned char *p = (const unsigned char *)*s;
	if (!*p) return 0;
	if (*p < 0x80) { (*s)++; return *p; }
	unsigned cp;
	int len;
	if      ((*p & 0xE0) == 0xC0) { len = 2; cp = *p & 0x1F; }
	else if ((*p & 0xF0) == 0xE0) { len = 3; cp = *p & 0x0F; }
	else if ((*p & 0xF8) == 0xF0) { len = 4; cp = *p & 0x07; }
	else { (*s)++; return 0xFFFD; }
	for (int i = 1; i < len; i++)
	{
		if ((p[i] & 0xC0) != 0x80) { (*s)++; return 0xFFFD; }
		cp = (cp << 6) | (p[i] & 0x3F);
	}
	(*s) += len;
	return cp;
}

// Check if a byte is a valid UTF-8 lead byte (2/3/4-byte sequence)
static inline bool is_utf8_lead(int byte)
{
	return (byte & 0xE0) == 0xC0 || (byte & 0xF0) == 0xE0 || (byte & 0xF8) == 0xF0;
}

// --- Pixel blending (same logic as blendpixel in graphics.cpp) ---

static inline void ft_blend(pixel *vid, int px, int py, int r, int g, int b, int a)
{
	if (px < 0 || py < 0 || px >= (XRES+BARSIZE) || py >= (YRES+MENUSIZE))
		return;
	pixel t = vid[py * (XRES+BARSIZE) + px];
	int tr = PIXR(t), tg = PIXG(t), tb = PIXB(t);
	r = (a * r + (255 - a) * tr) >> 8;
	g = (a * g + (255 - a) * tg) >> 8;
	b = (a * b + (255 - a) * tb) >> 8;
	vid[py * (XRES+BARSIZE) + px] = PIXRGB(r, g, b);
}

// --- Fill rectangle (same logic as fillrect) ---

static inline void ft_fillrect(pixel *vid, int x, int y, int w, int h, int r, int g, int b, int a)
{
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++)
			ft_blend(vid, x + i, y + j, r, g, b, a);
}

// --- Public API ---

bool FT_HasUnicode(const char *s)
{
	if (!s) return false;
	for (; *s; s++)
		if ((unsigned char)*s >= 0x80)
			return true;
	return false;
}

std::string FT_ConvertIcons(const std::string &s)
{
	std::string result;
	result.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++)
	{
		int b = (unsigned char)s[i];
		if (b == 0xEE && i + 2 < s.size())
		{
			std::string tmp(s.c_str() + i, 3);
			char icon = Format::ConvertFontIcon(tmp, 0);
			if (icon != 0)
			{
				result += icon;
			}
			// Skip the 3-byte sequence whether converted or not
			i += 2;
			continue;
		}
		result += s[i];
	}
	return result;
}

bool FT_Init(const char *fontPath, int fontSize)
{
	FT_Shutdown();
	if (FT_Init_FreeType(&ft_library))
		return false;
	if (FT_New_Face(ft_library, fontPath, 0, &ft_face))
	{
		FT_Done_FreeType(ft_library);
		ft_library = nullptr;
		return false;
	}
	FT_Set_Pixel_Sizes(ft_face, 0, fontSize);
	ft_size = fontSize;
	ft_ready = true;
	return true;
}

void FT_Shutdown()
{
	if (ft_face) { FT_Done_Face(ft_face); ft_face = nullptr; }
	if (ft_library) { FT_Done_FreeType(ft_library); ft_library = nullptr; }
	ft_ready = false;
}

bool FT_IsReady() { return ft_ready; }
int  FT_TextHeight() { return FONT_H + 2; }

int FT_TextWidth(const char *s)
{
	if (!ft_ready || !s) return 0;
	int x = 0, maxX = 0;
	const char *p = s;
	while (*p)
	{
		int b = (unsigned char)*p;
		if (b == '\n' || b == '\r') { if (x > maxX) maxX = x; x = 0; p++; continue; }
		if (b == '\b')   { p += 2; continue; }
		if (b == 0x0F) { p += 4; continue; }
		if (b == 0x0E || b == 0x01 || b == 0x02) { p++; continue; }
		if (b == 0xEE) {
			std::string tmp(p, 3);
			char icon = Format::ConvertFontIcon(tmp, 0);
			x += (icon != 0) ? font_data[font_ptrs[(unsigned char)icon]] : 0;
			p += 3;
			continue;
		}
		if (b >= 0x80 && !is_utf8_lead(b)) { p++; continue; }
		unsigned cp = utf8_decode(&p);
		if (cp == 0) break;
		if (FT_Load_Char(ft_face, cp, FT_LOAD_DEFAULT)) continue;
		x += ft_face->glyph->advance.x >> 6;
	}
	if (x > maxX) maxX = x;
	return maxX;
}

int FT_DrawText(pixel *vid, int x, int y, const char *s, int r, int g, int b, int a, bool noColor)
{
	if (!ft_ready || !s || !*s) return x;

	int startX = x;
	bool highlight = false, underline = false;
	int oR = r, oG = g, oB = b;
	const char *p = s;

	while (*p)
	{
		int byte0 = (unsigned char)*p;

		// Newlines
		if (byte0 == '\n' || byte0 == '\r')
		{
			x = startX;
			y += FONT_H + 2;
			p++;
			continue;
		}
		// Custom RGB color: \x0F R G B
		if (byte0 == 0x0F)
		{
			if (!p[1] || !p[2] || !p[3]) break;
			if (!noColor)
			{
				oR = r; oG = g; oB = b;
				r = (unsigned char)p[1];
				g = (unsigned char)p[2];
				b = (unsigned char)p[3];
			}
			p += 4;
			continue;
		}
		// Reset to original color
		if (byte0 == 0x0E)
		{
			r = oR; g = oG; b = oB;
			p++;
			continue;
		}
		// Highlight toggle
		if (byte0 == 0x01)
		{
			highlight = !highlight;
			p++;
			continue;
		}
		// Inline color codes: \bX
		if (byte0 == '\b')
		{
			if (!noColor && p[1])
			{
				switch (p[1])
				{
				case 'w': r = g = b = 255; break;
				case 'g': r = g = b = 192; break;
				case 'o': r = 255; g = 216; b = 32; break;
				case 'r': r = 255; g = b = 0; break;
				case 'l': r = 255; g = b = 75; break;
				case 'b': r = g = 0; b = 255; break;
				case 't': b = 255; g = 170; r = 32; break;
				case 'u': r = 147; g = 83; b = 211; break;
				case 'p': b = 100; g = 10; r = 100; break;
				case 'U': underline = !underline; break;
				}
			}
			p += 2;
			continue;
		}
		// Font icons: \xEE \x80/\x81 X
		if (byte0 == 0xEE)
		{
			std::string tmp(p, 3);
			char icon = Format::ConvertFontIcon(tmp, 0);
			if (icon != 0)
			{
				x = drawchar(vid, x, y, (unsigned char)icon, r, g, b, a);
				p += 3;
				continue;
			}
		}
		// Cursor mark
		if (byte0 == 0x02)
		{
			for (int dy = -1; dy < FONT_H; dy++)
				ft_blend(vid, x, y + dy, 255, 255, 255, 255);
			p++;
			continue;
		}

		// --- Non-ASCII: try UTF-8 first, fall back to bitmap ---
		if (byte0 >= 0x80)
		{
			const char *saved = p;
			unsigned cp = utf8_decode(&p);

			// If valid multi-byte UTF-8 was decoded, render via FreeType
			if (p > saved + 1 && cp != 0 && cp != 0xFFFD &&
			    !FT_Load_Char(ft_face, cp, FT_LOAD_RENDER))
			{
				FT_GlyphSlot glyph = ft_face->glyph;
				FT_Bitmap   *bmp   = &glyph->bitmap;
				int gw = glyph->advance.x >> 6;
				int bx = x + glyph->bitmap_left;
				int by = y;

				if (highlight)
					ft_fillrect(vid, x - 1, y - 2, gw + 1, FONT_H + 3, 0, 0, 255, 127);

				if (bmp->buffer)
				{
					for (int row = 0; row < bmp->rows; row++)
					{
						for (int col = 0; col < bmp->width; col++)
						{
							unsigned char val;
							if (bmp->pixel_mode == FT_PIXEL_MODE_MONO)
								val = (bmp->buffer[row * bmp->pitch + col / 8] & (0x80 >> (col & 7))) ? 255 : 0;
							else
								val = bmp->buffer[row * bmp->pitch + col];
							if (val == 0) continue;
							ft_blend(vid, bx + col, by + row, r, g, b, (val * a) / 255);
						}
					}
				}

				if (underline)
				{
					for (int i = 0; i < gw; i++)
						ft_blend(vid, x + i, y + FONT_H + 1, r, g, b, a);
				}
				x += gw;
				continue;
			}

			// Invalid UTF-8 or single byte -> render as bitmap glyph
			p = saved + 1;
			if (highlight && byte0 < 0x80)
				ft_fillrect(vid, x - 1, y - 2, font_data[font_ptrs[byte0]] + 1, FONT_H + 3, 0, 0, 255, 127);
			int newX = drawchar(vid, x, y, (unsigned char)byte0, r, g, b, a);
			if (underline)
			{
				for (int i = x; i < newX; i++)
					ft_blend(vid, i, y + FONT_H + 1, r, g, b, a);
			}
			x = newX;
			continue;
		}

		// --- ASCII: render via FreeType ---
		unsigned cp = utf8_decode(&p);
		if (cp == 0) break;
		if (FT_Load_Char(ft_face, cp, FT_LOAD_RENDER)) continue;

		FT_GlyphSlot glyph = ft_face->glyph;
		FT_Bitmap   *bmp   = &glyph->bitmap;
		int gw = glyph->advance.x >> 6;

		int bx = x + glyph->bitmap_left;
		int by = y;

		if (highlight)
			ft_fillrect(vid, x - 1, y - 2, gw + 1, FONT_H + 3, 0, 0, 255, 127);

		if (bmp->buffer)
		{
			for (int row = 0; row < bmp->rows; row++)
			{
				for (int col = 0; col < bmp->width; col++)
				{
					unsigned char val;
					if (bmp->pixel_mode == FT_PIXEL_MODE_MONO)
						val = (bmp->buffer[row * bmp->pitch + col / 8] & (0x80 >> (col & 7))) ? 255 : 0;
					else
						val = bmp->buffer[row * bmp->pitch + col];
					if (val == 0) continue;
					ft_blend(vid, bx + col, by + row, r, g, b, (val * a) / 255);
				}
			}
		}

		if (underline)
		{
			for (int i = 0; i < gw; i++)
				ft_blend(vid, x + i, y + FONT_H + 1, r, g, b, a);
		}

		x += gw;
	}
	return x;
}