#include "VideoBuffer.h"
#include <cmath>
#include <cstdlib>
#define INCLUDE_FONTDATA
#include "font.h"
#include "common/Format.h"
#include "common/tpt-minmax.h"
#include "graphics/FTText.h"

namespace gfx
{

VideoBuffer::VideoBuffer(int width, int height):
	width(width),
	height(height),
	clipRect1(0, 0),
	clipRect2(width, height)
{
	vid = new pixel[width*height];
	Clear();
}

VideoBuffer::~VideoBuffer()
{
	delete[] vid;
}

void VideoBuffer::Clear()
{
	std::fill(vid, vid+(width*height), 0);
}

void VideoBuffer::ClearRect(int x, int y, int w, int h)
{
	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x+w >= width)
		w = width-x;
	if (y+h >= height)
		h = height-y;
	if (w<0 || h<0)
		return;

	for (int i = 0; i < h; i++)
		std::fill(&vid[x+width*(y+i)], &vid[x+w+width*(y+i)], 0);
}

void VideoBuffer::CopyBufferInto(pixel* vidPaste, int vidWidth, int vidHeight, int x, int y)
{
	for (int i = 0; i < height && i < vidHeight - y; i++)
	{
		if (y+i >= 0)
			std::copy(&vid[width*i], &vid[width*(i+1)], &vidPaste[x+((y+i)*vidWidth)]);
	}
}

// like CopyBufferInto, but handles black pixels differently. It makes these partially transparent
void VideoBuffer::CopyBufferIntoWithPartialAlpha(pixel* vidPaste, int vidWidth, int vidHeight, int x, int y, int alpha)
{
	for (int j = 0; j < height && j < vidHeight - y; j++)
	{
		for (int i = 0; i < width && i < vidWidth - x; i++)
		{
			if (vid[j * width + i])
			{
				vidPaste[x + i + ((y + j) * vidWidth)] = vid[j * width + i];
			}
			else
			{
				pixel t = vidPaste[x + i + ((y + j) * vidWidth)];
				vidPaste[j * width + i] = PIXRGB(
					((255 - alpha) * PIXR(t)) >> 8,
					((255 - alpha) * PIXG(t)) >> 8,
					((255 - alpha) * PIXB(t)) >> 8
				);
			}
		}
	}
}

void VideoBuffer::CopyBufferFrom(pixel* vidFrom, int vidWidth, int vidHeight, int w, int h)
{
	// Invalid arguments
	if (w > vidWidth || h > vidHeight)
		return;

	// Copy from buffer is wider than our buffer
	if (w > clipRect2.X)
		w = clipRect2.X;
	for (int i = 0; i < h && i < clipRect2.Y; i++)
	{
		std::copy(&vidFrom[vidWidth*i], &vidFrom[vidWidth*i + w], &vid[clipRect2.X*i]);
	}
}

//This function does NO bounds checking
void VideoBuffer::DrawPixel(int x, int y, int r, int g, int b, int a)
{
	if (a == 0)
		return;
	if (x < clipRect1.X || y < clipRect1.Y || x >= clipRect2.X || y >= clipRect2.Y)
		return;
	if (a != 255)
	{
		pixel t = vid[y*width+x];
		r = (a*r + (255-a)*PIXR(t)) >> 8;
		g = (a*g + (255-a)*PIXG(t)) >> 8;
		b = (a*b + (255-a)*PIXB(t)) >> 8;
	}
	vid[y*width+x] = PIXRGB(r,g,b);
}

//This function does NO bounds checking
void VideoBuffer::DrawPixel(int x, int y, ARGBColour color)
{
	DrawPixel(x, y, COLR(color), COLG(color), COLB(color), COLA(color));
}

//This function does NO bounds checking
void VideoBuffer::XorPixel(int x, int y)
{
	if (x < clipRect1.X || y < clipRect1.Y || x >= clipRect2.X || y >= clipRect2.Y)
		return;
	int c = vid[y*width+x];
	c = PIXB(c) + 3 * PIXG(c) + 2 * PIXR(c);
	if (c < 512)
		vid[y*width+x] = PIXPACK(0xC0C0C0);
	else
		vid[y*width+x] = PIXPACK(0x404040);
}

void VideoBuffer::DrawLine(int x1, int y1, int x2, int y2, int r, int g, int b, int a)
{
	int dx, dy, startX, startY, e, x, y;
	bool reverseXY = false;

	dx = std::abs(x1-x2);
	dy = std::abs(y1-y2);
	startX = (x1<x2) ? 1 : -1;
	startY = (y1<y2) ? 1 : -1;
	x = x1;
	y = y1;

	if (dy > dx)
	{
		dx = dx + dy;
		dy = dx - dy;
		dx = dx - dy;
		reverseXY = true;
	}

	e = (dy<<2) - dx;
	for (int i = 0; i <= dx; i++)
	{
		DrawPixel(x, y, r, g, b, a);
		if (e >= 0)
		{
			if (reverseXY)
				x = x + startX;
			else
				y = y + startY;
			e = e - (dx<<2);
		}
		if (reverseXY)
			y = y + startY;
		else
			x = x + startX;
		e = e + (dy<<2);
	}
}

void VideoBuffer::DrawLine(int x1, int y1, int x2, int y2, ARGBColour color)
{
	DrawLine(x1, y1, x2, y2, COLR(color), COLG(color), COLB(color), COLA(color));
}

void VideoBuffer::XorLine(int x1, int y1, int x2, int y2)
{
	int dx, dy, startX, startY, e, x, y;
	bool reverseXY = false;

	dx = std::abs(x1-x2);
	dy = std::abs(y1-y2);
	startX = (x1<x2) ? 1 : -1;
	startY = (y1<y2) ? 1 : -1;
	x = x1;
	y = y1;

	if (dy > dx)
	{
		dx = dx + dy;
		dy = dx - dy;
		dx = dx - dy;
		reverseXY = true;
	}

	e = (dy<<2) - dx;
	for (int i = 0; i <= dx; i++)
	{
		XorPixel(x, y);
		if (e >= 0)
		{
			if (reverseXY)
				x = x + startX;
			else
				y = y + startY;
			e = e - (dx<<2);
		}
		if (reverseXY)
			y = y + startY;
		else
			x = x + startX;
		e = e + (dy<<2);
	}
}

void VideoBuffer::DrawRect(int x, int y, int w, int h, int r, int g, int b, int a)
{
	if (x < 0)
	{
		if (x+w-1 >= 0)
		{
			w += x;
			x = 0;
		}
		else
			return;
	}
	if (y < 0)
	{
		if (y+h-1 >= 0)
		{
			h += y + 1;
			y = -1;
		}
		else
			return;
	}
	for (int i = 0; i < w && x+i < width; i++)
	{
		if (y >= 0 && y < height)
			DrawPixel(x+i, y, r, g, b, a);
		if (y+h-1 < height)
			DrawPixel(x+i, y+h-1, r, g, b, a);
	}
	for (int i = 1; i < h-1 && y+i < height; i++)
	{
		if (x < width)
			DrawPixel(x, y+i, r, g, b, a);
		if (x+w <= width)
			DrawPixel(x+w-1, y+i, r, g, b, a);
	}
}

void VideoBuffer::DrawRect(int x, int y, int w, int h, ARGBColour color)
{
	DrawRect(x, y, w, h, COLR(color), COLG(color), COLB(color), COLA(color));
}

void VideoBuffer::FillRect(int x, int y, int w, int h, int r, int g, int b, int a)
{
	if (x < 0)
	{
		if (x+w >= 0)
		{
			w += x;
			x = 0;
		}
		else
			return;
	}
	if (y < 0)
	{
		if (y+h >= 0)
		{
			h += y;
			y = 0;
		}
		else
			return;
	}
	if (x+w >= width)
		w = width-x;
	if (y+h >= height)
		h = height-y;

	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++)
			DrawPixel(x+i, y+j, r, g, b, a);
}

void VideoBuffer::FillRect(int x, int y, int w, int h, ARGBColour color)
{
	FillRect(x, y, w, h, COLR(color), COLG(color), COLB(color), COLA(color));
}

void VideoBuffer::DrawCircle(int x, int y, int rx, int ry, int r, int g, int b, int a)
{
	int tempy = y;
	if (!rx)
	{
		for (int j = -ry; j <= ry; j++)
			DrawPixel(x, y + j, r, g, b, a);
		return;
	}
	for (int i = x - rx; i <= x; i++)
	{
		int oldy = tempy;
		while (std::pow(i - x, 2.0) * std::pow(ry, 2.0) + std::pow(tempy - y, 2.0) * std::pow(rx, 2.0) <= std::pow(rx, 2.0) * std::pow(ry, 2.0))
			tempy = tempy - 1;
		tempy = tempy + 1;
		if (oldy != tempy)
			oldy--;
		for (int j = tempy; j <= oldy; j++)
		{
			int i2 = 2 * x - i, j2 = 2 * y - j;
			DrawPixel(i, j, r, g, b, a);
			if (i2 != i)
				DrawPixel(i2, j, r, g, b, a);
			if (j2 != j)
				DrawPixel(i, j2, r, g, b, a);
			if (i2 != i && j2 != j)
				DrawPixel(i2, j2, r, g, b, a);
		}
	}
}

void VideoBuffer::FillCircle(int x, int y, int rx, int ry, int r, int g, int b, int a)
{
	int tempy = y;
	if (!rx)
	{
		for (int j = -ry; j <= ry; j++)
			DrawPixel(x, y + j, r, g, b, a);
		return;
	}
	for (int i = x - rx; i <= x; i++)
	{
		while (std::pow(i - x, 2.0) * std::pow(ry, 2.0) + std::pow(tempy - y, 2.0) * std::pow(rx, 2.0) <= std::pow(rx, 2.0) * std::pow(ry, 2.0))
			tempy = tempy - 1;
		tempy = tempy + 1;
		int jmax = 2 * y - tempy;
		for (int j = tempy; j <= jmax; j++)
		{
			DrawPixel(i, j, r, g, b, a);
			if (i != x)
				DrawPixel(2 * x - i, j, r, g, b, a);
		}
	}
}

int VideoBuffer::DrawChar(int x, int y, unsigned char c, int r, int g, int b, int a, bool modifiedColor)
{
	if (c >= 0x80)
		return 0;
	int bn = 0, ba = 0;
	unsigned char *rp = font_data + font_ptrs[c];
	signed char w = *(rp++);
	unsigned char flags = *(rp++);
	signed char t = (flags&0x4) ? -(flags&0x3) : flags&0x3;
	signed char l = (flags&0x20) ? -((flags>>3)&0x3) : (flags>>3)&0x3;
	flags >>= 6;
	if ((flags&0x2))
	{
		if (!modifiedColor)
		{
			a = *(rp++);
			r = *(rp++);
			g = *(rp++);
			b = *(rp++);
		}
		else
			rp += 4;
	}
	for (int j = 0; j < FONT_H; j++)
		for (int i = 0; i < w && i < FONT_W; i++)
		{
			if (!bn)
			{
				ba = *(rp++);
				bn = 8;
			}
			if (x+i+l >= 0 && y+j+t >= 0 && x+i+l < width && y+j+t < height)
				DrawPixel(x+i+l, y+j+t, r, g, b, ((ba&3)*a)/3);
			ba >>= 2;
			bn -= 2;
		}
	if (flags&0x1)
		return std::max(x+w, DrawChar(x, y, c+1, r, g, b, a, modifiedColor));
	return x + w;
}

int VideoBuffer::DrawChar(int x, int y, unsigned char c, ARGBColour color, bool modifiedColor)
{
	return DrawChar(x, y, c, COLR(color), COLG(color), COLB(color), COLA(color), modifiedColor);
}

int VideoBuffer::DrawString(int x, int y, const std::string &s, int r, int g, int b, int a)
{
	if (a == 0)
		return x;
	if (FT_IsReady() && FT_HasUnicode(s.c_str()))
		return FT_DrawText(vid, x, y, FT_ConvertIcons(s).c_str(), r, g, b, a);

	int startX = x;
	bool highlight = false, underline = false, modifiedColor = false, didNewline = false;
	int oldR = r, oldG = g, oldB = b;
	for (size_t i = 0; i < s.length(); i++)
	{
		char c = s[i];
		switch (c)
		{
		case '\r':
			didNewline = true;
		case '\n':
			x = startX;
			y += FONT_H+2;
			if (highlight && s.length() > i+1 && (s[i+1] == '\n' || s[i+1] == '\r' || (s.length() > i+2 && s[i+1] == '\x01' && (s[i+2] == '\n' || s[i+2] == '\r'))))
			{
				FillRect(x, y-2, font_data[font_ptrs[' ']], FONT_H+2, 0, 0, 255, 127);
			}
			break;
		case '\x0F':
			if (s.length() < i+3)
				break;
			oldR = r;
			oldG = g;
			oldB = b;
			r = (unsigned char)s[i+1];
			g = (unsigned char)s[i+2];
			b = (unsigned char)s[i+3];
			modifiedColor = true;
			i += 3;
			break;
		case '\x0E':
			r = oldR;
			g = oldG;
			b = oldB;
			modifiedColor = false;
			break;
		case '\x01':
			highlight = !highlight;
			break;
		case '\x02':
			DrawLine(x, y-1, x, y+FONT_H-2, 255, 255, 255, 255);
			break;
		// old color codes, expect a single character after them
		case '\b':
			if (s.length() < i+1)
				break;
			oldR = r;
			oldG = g;
			oldB = b;
			switch (s[i+1])
			{
			case 'w':
				r = g = b = 255;
				break;
			case 'g':
				r = g = b = 192;
				break;
			case 'o':
				r = 255;
				g = 216;
				b = 32;
				break;
			case 'r':
				r = 255;
				g = b = 0;
				break;
			case 'l':
				r = 255;
				g = b = 75;
				break;
			case 'b':
				r = g = 0;
				b = 255;
				break;
			case 't':
				b = 255;
				g = 170;
				r = 32;
				break;
			case 'u':
				r = 147;
				g = 83;
				b = 211;
				break;
			case 'p':
				b = 100;
				g = 10;
				r = 100;
				break;
			case 'U':
				underline = !underline;
				break;
			}
			i++;
			break;
		case '\xEE':
		{
			char convertedC = Format::ConvertFontIcon(s, i);
			if (convertedC != 0)
			{
				c = convertedC;
				i += 2;
			}
		}
		default:
			int oldX = x;
			x = DrawChar(x, y, c, r, g, b, a, modifiedColor);
			if (didNewline && c == ' ')
			{
				x = oldX;
			}
			else if (highlight)
			{
				FillRect(oldX, y-2, font_data[font_ptrs[' ']], FONT_H+2, 0, 0, 255, 127);
			}
			if (underline)
			{
				DrawLine(oldX, y + FONT_H, x - 1, y + FONT_H, r, g, b, a);
			}
			didNewline = false;
		}
	}
	return x;
}

int VideoBuffer::DrawString(int x, int y, const std::string & s, ARGBColour color)
{
	return DrawString(x, y, s, COLR(color), COLG(color), COLB(color), COLA(color));
}

// static method that returns the width of a character
signed char VideoBuffer::CharSize(unsigned char c)
{
	if (c >= 0x80)
		return 0;
	short font_ptr = font_ptrs[static_cast<int>(c)];
	if (font_data[font_ptr+1]&0x40)
		return std::max((signed char)font_data[font_ptr], CharSize(c+1));
	return (font_data[font_ptr+1]&0x40) ? 0 : font_data[font_ptr];
}

// static method that returns the width and height of a string
Point VideoBuffer::TextSize(std::string s)
{
	if (FT_IsReady() && FT_HasUnicode(s.c_str()))
	{
		int w = FT_TextWidth(s.c_str());
		int h = FT_TextHeight();
		for (size_t i = 0; i < s.length(); i++)
			if (s[i] == '\n') h += FT_TextHeight() + 2;
		return Point(w, h);
	}

	int x = 0;
	int width = 0;
	int height = FONT_H;
	for (size_t i = 0; i < s.length(); i++)
	{
		char c = s[i];
		switch (c)
		{
		case '\n':
		case '\r':
			if (x > width)
				width = x;
			x = 0;
			height += FONT_H+2;
			break;
		case '\x0F':
			if (s.length() < i+3)
				break;
			i += 3;
			break;
		case '\x0E':
		case '\x01':
		case '\x02':
			break;
		case '\b':
			if (s.length() < i+1)
				break;
			i++;
			break;
		case '\xEE':
		{
			char convertedC = Format::ConvertFontIcon(s, i);
			if (convertedC != 0)
			{
				c = convertedC;
				i += 2;
			}
		}
		default:
			x += CharSize(static_cast<unsigned char>(c));
		}
	}
	if (x > width)
		width = x;
	return Point(width, height);
}

void VideoBuffer::DrawImage(pixel *img, int x, int y, int w, int h, int a)
{	
	int startX = 0;
	if (!img)
		return;
	// Adjust height to prevent drawing off the bottom
	if (y + h > clipRect2.Y)
		h = ((clipRect2.Y)-y)-1;
	// Too big
	if (x + w > clipRect2.X)
		return;

	// Starts off the top of the video buffer, adjust
	if (y < clipRect1.Y && clipRect1.Y - y < h)
	{
		img += clipRect1.Y - y * w;
		h += y - clipRect1.Y;
		y = clipRect1.Y;
	}
	// Starts off the left side of the video buffer, adjust
	if (x < clipRect1.X && clipRect1.X - x < w)
	{
		startX = clipRect1.X - x;
	}

	if (!h || y < clipRect1.Y || !w)
		return;
	if (a >= 255)
		for (int j = 0; j < h; j++)
		{
			img += startX;
			for (int i = startX; i < w; i++)
			{
				vid[(y+j)*(width)+(x+i)] = *img;
				img++;
			}
		}
	else
	{
		int r, g, b;
		for (int j = 0; j < h; j++)
		{
			img += startX;
			for (int i = startX; i < w; i++)
			{
				r = PIXR(*img);
				g = PIXG(*img);
				b = PIXB(*img);
				DrawPixel(x+i, y+j, r, g, b, a);
				img++;
			}
		}
	}
}

void VideoBuffer::SwapClipRect(Point & upperLeft, Point & bottomRight)
{
	Point clipRect1 = upperLeft;
	Point clipRect2 = bottomRight;
	upperLeft = this->clipRect1;
	bottomRight = this->clipRect2;
	this->clipRect1 = clipRect1;
	this->clipRect2 = clipRect2;
}

void VideoBuffer::ResetClipRect()
{
	clipRect1 = Point(0, 0);
	clipRect2 = Point(width, height);
}

}
