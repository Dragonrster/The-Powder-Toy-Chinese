/**
 * Powder Toy - graphics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/tpt-minmax.h"
#include <cmath>
#include <sstream>
#include <bzlib.h>
#include <climits>

#include "defines.h"
#include "interface.h"
#include "powder.h"
#define INCLUDE_SHADERS
#include "graphics.h"
#include "graphics/FTText.h"
#include "powdergraphics.h"
#define INCLUDE_FONTDATA
#include "font.h"
#include "misc.h"
#include "hmap.h"
#include "luaconsole.h"
#include "hud.h"

#include "common/Format.h"
#include "common/tpt-math.h"
#include "game/Brush.h"
#include "game/Menus.h"
#include "game/Sign.h"
#include "graphics/Renderer.h"
#include "interface/Engine.h"
#include "lua/LuaSmartRef.h"
#include "simulation/Simulation.h"
#include "simulation/Tool.h"
#include "simulation/WallNumbers.h"
#include "simulation/ToolNumbers.h"
#include "simulation/GolNumbers.h"
#include "simulation/elements/EMP.h"
#include "simulation/elements/FIGH.h"
#include "simulation/elements/LIFE.h"
#include "simulation/elements/STKM.h"

#include "gui/game/PowderToy.h"

gcache_item *graphicscache;

pixel sampleColor = 0;

unsigned char fire_r[YRES/CELL][XRES/CELL];
unsigned char fire_g[YRES/CELL][XRES/CELL];
unsigned char fire_b[YRES/CELL][XRES/CELL];

unsigned int fire_alpha[CELL*3][CELL*3];
float fireIntensity;
pixel *pers_bg;

char * flm_data;
int flm_data_points = 4;
pixel flm_data_colours[] = {PIXPACK(0xAF9F0F), PIXPACK(0xDFBF6F), PIXPACK(0x60300F), PIXPACK(0x000000)};
float flm_data_pos[] = {1.0f, 0.9f, 0.5f, 0.0f};

char * plasma_data;
int plasma_data_points = 5;
pixel plasma_data_colours[] = {PIXPACK(0xAFFFFF), PIXPACK(0xAFFFFF), PIXPACK(0x301060), PIXPACK(0x301040), PIXPACK(0x000000)};
float plasma_data_pos[] = {1.0f, 0.9f, 0.5f, 0.25, 0.0f};

int clip_rect_x1 = 0;
int clip_rect_y1 = 0;
int clip_rect_x2 = VIDXRES;
int clip_rect_y2 = VIDYRES;



//an easy way to draw a blob
void drawblob(pixel *vid, int x, int y, unsigned char cr, unsigned char cg, unsigned char cb)
{
	blendpixel(vid, x+1, y, cr, cg, cb, 112);
	blendpixel(vid, x-1, y, cr, cg, cb, 112);
	blendpixel(vid, x, y+1, cr, cg, cb, 112);
	blendpixel(vid, x, y-1, cr, cg, cb, 112);

	blendpixel(vid, x+1, y-1, cr, cg, cb, 64);
	blendpixel(vid, x-1, y-1, cr, cg, cb, 64);
	blendpixel(vid, x+1, y+1, cr, cg, cb, 64);
	blendpixel(vid, x-1, y+1, cr, cg, cb, 64);
}

//draws the background and correctly colored text for each button
void draw_tool_button(pixel *vid_buf, int x, int y, pixel color, std::string name)
{
	fillrect(vid_buf, x, y, 27, 15, PIXR(color), PIXG(color), PIXB(color), 255);

	int textColor = 0;
	if (PIXB(color) + 3*PIXG(color) + 2*PIXR(color) < 544)
		textColor = 255;
	drawtext(vid_buf, x+14-textwidth(name.c_str())/2, y+4, name.c_str(), textColor, textColor, textColor, 255);
}

//draws walls and elements for menu
int draw_tool_xy(pixel *vid_buf, int x, int y, Tool* current)
{
	int i, j;
	if (x > menuStartPosition-28 || x < -26)
		return 26;
	int toolID = current->GetID();
	if (current->GetType() == ELEMENT_TOOL)
	{
		draw_tool_button(vid_buf, x, y, PIXPACK(current->GetColor()), toolID == 0 ? "" : current->GetName());

		//special case for erase tool
		if (!toolID)
		{
			for (j=4; j<12; j++)
			{
				drawpixel(vid_buf, x+j+6, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x+j+7, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x-j+21, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x-j+22, y+j, 0xFF, 0, 0, 255);
			}
		}
	}
	else if (current->GetType() == WALL_TOOL)
	{
		int ds = wallTypes[toolID].drawstyle;
		pixel color = PIXPACK(wallTypes[toolID].colour);
		pixel glowColor = PIXPACK(wallTypes[toolID].eglow);
		
		if (ds==1)
		{
			for (j=1; j<15; j+=2)
				for (i=1+(1&(j>>1)); i<27; i+=2)
					drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
		}
		else if (ds==2)
		{
			for (j=1; j<15; j+=2)
				for (i=1; i<27; i+=2)
					drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
		}
		else if (ds==3)
		{
			for (j=1; j<15; j++)
				for (i=1; i<27; i++)
					drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
		}
		else if (ds==4)
		{
			for (j=1; j<15; j++)
				for (i=1; i<27; i++)
					if(i%CELL == j%CELL)
						drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
					else if  (i%CELL == (j%CELL)+1 || (i%CELL == 0 && j%CELL == CELL-1))
						drawpixel(vid_buf, x+i, y+j, PIXR(glowColor), PIXG(glowColor), PIXB(glowColor), 255);
					else
						drawpixel(vid_buf, x+i, y+j, 0x20, 0x20, 0x20, 255);
		}
		else
		switch (toolID)
		{
		case WL_WALLELEC:
			for (j=1; j<15; j++)
			{
				for (i=1; i<27; i++)
				{
					if (!(i%2) && !(j%2))
					{
						drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
					}
					else
					{
						drawpixel(vid_buf, x+i, y+j, 0x80, 0x80, 0x80, 255);
					}
				}
			}
			break;
		case WL_EWALL:
			for (j=1; j<15; j++)
			{
				for (i=1; i<6+j; i++)
				{
					if (!(i&j&1))
					{
						drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
					}
				}
				for (; i<27; i++)
				{
					if (i&j&1)
					{
						drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
					}
				}
			}
			break;
		case WL_EHOLE:
		case WL_STASIS:
			for (j=1; j<15; j++)
			{
				for (i=1; i<6+j; i++)
				{
					if (i&j&1)
					{
						drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
					}
				}
				for (; i<27; i++)
				{
					if (!(i&j&1))
					{
						drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
					}
				}
			}
			break;
		case WL_STREAM:
			for (j=1; j<15; j++)
			{
				for (i=1; i<27; i++)
				{
					if (i==1||i==26||j==1||j==14)
						drawpixel(vid_buf, x+i, y+j, 0xA0, 0xA0, 0xA0, 255);
					else
						drawpixel(vid_buf, x+i, y+j, 0, 0, 0, 255);
					drawtext(vid_buf, x+4, y+3, "\x8D", 255, 255, 255, 255);
				}
			}
			for (i=9; i<27; i++)
			{
				drawpixel(vid_buf, x+i, y+8+(int)(3.9f*cos(i*0.3f)), 255, 255, 255, 255);
			}
			break;
		case WL_ERASE:
			for (j=1; j<15; j+=2)
			{
				for (i=1+(1&(j>>1)); i<13; i+=2)
				{
					drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
				}
			}
			for (j=1; j<15; j++)
			{
				for (i=14; i<27; i++)
				{
					drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
				}
			}

			//X in middle
			for (j=4; j<12; j++)
			{
				drawpixel(vid_buf, x+j+6, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x+j+7, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x-j+21, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x-j+22, y+j, 0xFF, 0, 0, 255);
			}
			break;
		case WL_ERASEALL:
			for (j=1; j<15; j++)
			{
				int r = 100, g = 150, b = 50;
				int rd = 1, gd = -1, bd = -1;
				for (i=1; i<27; i++)
				{
					int rc, gc, bc;
					r+=15*rd; g+=15*gd; b+=15*bd;
					if (r > 200) rd = -1;
					if (g > 200) gd = -1;
					if (b > 200) bd = -1;
					if (r < 15) rd = 1;
					if (g < 15) gd = 1;
					if (b < 15) bd = 1;
					rc = std::max(0,r); gc = std::max(0,g); bc = std::max(0,b);
					rc = std::min(150,rc); gc = std::min(200,gc); bc = std::min(200,bc);
					drawpixel(vid_buf, x+i, y+j, rc, gc, bc, 255);
				}
			}

			//double X in middle
			for (j=4; j<12; j++)
			{

				drawpixel(vid_buf, x+j+0, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x+j+1, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x-j+15, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x-j+16, y+j, 0xFF, 0, 0, 255);

				drawpixel(vid_buf, x+j+11, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x+j+12, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x-j+26, y+j, 0xFF, 0, 0, 255);
				drawpixel(vid_buf, x-j+27, y+j, 0xFF, 0, 0, 255);
			}
			break;
		default:
			draw_tool_button(vid_buf, x, y, color, "");
		}
	}
	else if (current->GetType() == TOOL_TOOL)
	{
		if (toolID == TOOL_SIGN)
		{
			for (j=1; j<15; j++)
			{
				for (i=1; i<27; i++)
				{
					if (i==1||i==26||j==1||j==14)
						drawpixel(vid_buf, x+i, y+j, 0xA0, 0xA0, 0xA0, 255);
					else
						drawpixel(vid_buf, x+i, y+j, 0, 0, 0, 255);
				}
			}
			drawtext(vid_buf, x+9, y+5, "\xA1", 32, 64, 128, 255);
			drawtext(vid_buf, x+9, y+5, "\xA0", 255, 255, 255, 255);
		}
		else
			draw_tool_button(vid_buf, x, y, PIXPACK(current->GetColor()), current->GetName());
	}
	else if (current->GetType() == DECO_TOOL)
	{
		ARGBColour color = decoTypes[toolID].color;
		for (j=1; j<15; j++)
		{
			for (i=1; i<27; i++)
			{
				if (toolID == DECO_LIGHTEN)
					drawpixel(vid_buf, x+i, y+j, COLR(color)-10*j, COLG(color)-10*j, COLB(color)-10*j, 255);
				else if (toolID == DECO_DARKEN)
					drawpixel(vid_buf, x+i, y+j, COLR(color)+10*j, COLG(color)+10*j, COLB(color)+10*j, 255);
				else if (toolID == DECO_SMUDGE)
					drawpixel(vid_buf, x+i, y+j, COLR(color), COLG(color)-5*i, COLB(color)+5*i, 255);
				else if (toolID == DECO_DRAW || toolID == DECO_CLEAR)
					drawpixel(vid_buf, x+i, y+j, COLR(decocolor), COLG(decocolor), COLB(decocolor), 255);
				else
					drawpixel(vid_buf, x+i, y+j, PIXR(color), PIXG(color), PIXB(color), 255);
			}
		}

		if (toolID == DECO_CLEAR)
		{
			int cr = (COLR(decocolor)+127)%256;
			int cg = (COLG(decocolor)+127)%256;
			int cb = (COLB(decocolor)+127)%256;
			for (j=4; j<12; j++)
			{
				drawpixel(vid_buf, x+j+6, y+j, cr, cg, cb, 255);
				drawpixel(vid_buf, x+j+7, y+j, cr, cg, cb, 255);
				drawpixel(vid_buf, x-j+21, y+j, cr, cg, cb, 255);
				drawpixel(vid_buf, x-j+22, y+j, cr, cg, cb, 255);
			}
		}
		else if (toolID == DECO_ADD)
			drawtext(vid_buf, x+12, y+5, "+", COLR(decocolor), COLG(decocolor), COLB(decocolor), 255);
		else if (toolID == DECO_SUBTRACT)
			drawtext(vid_buf, x+12, y+5, "-", COLR(decocolor), COLG(decocolor), COLB(decocolor), 255);
		else if (toolID == DECO_MULTIPLY)
			drawtext(vid_buf, x+12, y+4, "x", COLR(decocolor), COLG(decocolor), COLB(decocolor), 255);
		else if (toolID == DECO_DIVIDE)
			drawtext(vid_buf, x+12, y+5, "/", COLR(decocolor), COLG(decocolor), COLB(decocolor), 255);
	}
	// No special logic or graphics for these
	else if (current->GetType() == GOL_TOOL || current->GetType() == FAV_MENU_BUTTON || current->GetType() == HUD_MENU_BUTTON
			  || current->GetType() == CUSTOM_TOOL)
	{
		draw_tool_button(vid_buf, x, y, PIXPACK(current->GetColor()), current->GetName());
	}
	return 26;
}

#ifndef TOUCHUI
int DrawMenus(pixel *vid_buf, int hover, int mouseY)
{
	int y = YRES+MENUSIZE-32, ret = -1;
	for (int i = SC_TOTAL-1; i >= 0; i--)
	{
		if (menuSections[i]->enabled)
		{
			drawrect(vid_buf, (XRES+BARSIZE)-16, y, 14, 14, 255, 255, 255, 255);

			if (hover == i || (i == SC_FAV && (hover == SC_FAV2 || hover == SC_HUD)))
			{
				fillrect(vid_buf, (XRES+BARSIZE)-16, y, 14, 14, 255, 255, 255, 255);
				drawchar(vid_buf, (XRES+BARSIZE)-13, y+2, menuSections[i]->icon, 0, 0, 0, 255);
			}
			else
			{
				drawchar(vid_buf, (XRES+BARSIZE)-13, y+2, menuSections[i]->icon, 255, 255, 255, 255);
			}

			if (mouseY >= y && mouseY < y+16)
				ret = i;
			y -= 16;
		}
	}
	return ret;
}
#else

bool draggingMenuSections = false;
const int menuIconHeight = 20;
int menuOffset = -menuIconHeight * SC_POWDERS, menuStart = 0;
int DrawMenusTouch(pixel *vid_buf, int b, int bq, int mx, int my)
{
	int xStart = menuStartPosition-2, y = YRES-70+menuOffset+1;
	bool checkSearchMenu = false;

	// draw menu icons
	for (int i = 0; i < SC_TOTAL; i++)
	{
		if (menuSections[i]->enabled)
		{
			if (y >= YRES-70-16*4-menuIconHeight/3+2 && y <= YRES-70+16*3+menuIconHeight/2+3)
			{
				drawrect(vid_buf, xStart+1, y, menuIconWidth-3, menuIconHeight-2, 255, 255, 255, 255);
				drawchar(vid_buf, xStart+4, y+(menuIconHeight-13)/2, menuSections[i]->icon, 255, 255, 255, 255);
			}
			y += menuIconHeight;
		}
	}
	// fade out the icons on the edges
	for (int i = 0; i < 31; i++)
	{
		for (int x = xStart; x < xStart+menuIconWidth; x++)
		{
			drawpixel(vid_buf, x, YRES-70-16*3+i, 0, 0, 0, 255-i*8);
			drawpixel(vid_buf, x, YRES-70+16*4-i-1, 0, 0, 0, 255-i*8);
		}
	}
	fillrect(vid_buf, xStart, YRES-70-16*3-menuIconHeight-1, menuIconWidth, menuIconHeight, 0, 0, 0, 255);
	fillrect(vid_buf, xStart, YRES-70+16*4, menuIconWidth, menuIconHeight, 0, 0, 0, 255);

	// green box
	drawrect(vid_buf, xStart+1, YRES-70+1, menuIconWidth-3, menuIconHeight-2, 0, 255, 0, 255);

	// box around the entire thing
	draw_line(vid_buf, xStart-1, YRES-70-16*3-1, xStart+menuIconWidth, YRES-70-16*3-1, 150, 150, 150, XRES+BARSIZE);
	draw_line(vid_buf, xStart-1, YRES-70+16*4, xStart+menuIconWidth, YRES-70+16*4, 150, 150, 150, XRES+BARSIZE);
	draw_line(vid_buf, xStart-1, YRES-70-16*3-1, xStart-1, YRES-70+16*4, 150, 150, 150, XRES+BARSIZE);
	draw_line(vid_buf, xStart+menuIconWidth, YRES-70-16*3-1, xStart+menuIconWidth, YRES-70+16*4, 150, 150, 150, XRES+BARSIZE);

	// scrolling logic
	if (b && !bq && mx > xStart && my >= YRES-70-16*3 && my <= YRES-70+16*4)
	{
		draggingMenuSections = true;
		menuStart = my-menuOffset;
	}
	else if (b && draggingMenuSections)
	{
		int menuSize = (GetNumMenus()-1)*-menuIconHeight;
		menuOffset = my-menuStart;
		if (menuOffset > 0)
			menuOffset = 0;
		else if (menuOffset < menuSize)
			menuOffset = menuSize;
	}
	else if (draggingMenuSections)
	{
		draggingMenuSections = false;
		if (bq)
		{
			menuOffset = (menuOffset-(menuIconHeight/2))/menuIconHeight*menuIconHeight;
			checkSearchMenu = true;
		}
	}

	if (draggingMenuSections || checkSearchMenu)
	{
		// figure out which menu section is selected based on menuOffset, accounting for menusections that may not be enabled
		int menu = (menuOffset-(menuIconHeight/2))/-menuIconHeight, count = -1;
		for (int i = 0; i < SC_TOTAL; i++)
			if (menuSections[i]->enabled)
			{
				count++;
				// found the selected menu
				if (count == menu)
				{
					// if search menu was scrolled to, open elemnt search
					if (checkSearchMenu && i == SC_SEARCH)
					{
						element_search_ui(vid_buf, &activeTools[0], &activeTools[1], &activeTools[2]);
						int menuSection = GetMenuSection(activeTools[0]);
						// handle error (this shouldn't ever happen)
						if (menuSection == -1)
							return i;
						int offset = 0;
						// scroll back to menu with selected element in it (we don't want to leave search menu selected)
						for (int i = 0; i < SC_TOTAL; i++)
						{
							if (i == menuSection)
							{
								menuOffset = offset;
								break;
							}
							if (menuSections[i]->enabled)
								offset -= menuIconHeight;
						}
						return menuSection;
					}
					return i;
				}
			}

	}
	return -1;
}
#endif

//draws a pixel, identical to blendpixel(), except blendpixel has OpenGL support
void drawpixel(pixel *vid, int x, int y, int r, int g, int b, int a)
{
#ifdef PIXALPHA
	pixel t;
	if (x < clip_rect_x1 || y < clip_rect_y1 || x >= clip_rect_x2 || y >= clip_rect_y2)
		return;
	if (a!=255)
	{
		t = vid[y*(XRES+BARSIZE)+x];
		r = (a*r + (255-a)*PIXR(t)) >> 8;
		g = (a*g + (255-a)*PIXG(t)) >> 8;
		b = (a*b + (255-a)*PIXB(t)) >> 8;
		a = a > PIXA(t) ? a : PIXA(t);
	}
	vid[y*(XRES+BARSIZE)+x] = PIXRGBA(r,g,b,a);
#else
	pixel t;
	if (x < clip_rect_x1 || y < clip_rect_y1 || x >= clip_rect_x2 || y >= clip_rect_y2 || a == 0)
		return;
	if (a!=255)
	{
		t = vid[y*(XRES+BARSIZE)+x];
		r = (a*r + (255-a)*PIXR(t)) >> 8;
		g = (a*g + (255-a)*PIXG(t)) >> 8;
		b = (a*b + (255-a)*PIXB(t)) >> 8;
	}
	vid[y*(XRES+BARSIZE)+x] = PIXRGB(r,g,b);
#endif
}

int drawchar(pixel *vid, int x, int y, int c, int r, int g, int b, int a)
{
	if (c < 0 || c >= 0x80)
		return x;
	int bn = 0, ba = 0;
	unsigned char *rp = font_data + font_ptrs[c];
	signed char w = *(rp++);
	unsigned char flags = *(rp++);
	signed char t = (flags&0x4) ? -(flags&0x3) : flags&0x3;
	signed char l = (flags&0x20) ? -((flags>>3)&0x3) : (flags>>3)&0x3;
	flags >>= 6;
	if (flags&0x2)
	{
		/*a = *(rp++);
		r = *(rp++);
		g = *(rp++);
		b = *(rp++);*/
		rp += 4;
	}
	for (int j = 0; j < FONT_H; j++)
		for (int i = 0; i < w && i<FONT_W; i++)
		{
			if (!bn)
			{
				ba = *(rp++);
				bn = 8;
			}
			drawpixel(vid, x+i+l, y+j+t, r, g, b, ((ba&3)*a)/3);
			ba >>= 2;
			bn -= 2;
		}
	return x + (flags&0x01 ? 0 : w);
}

int addchar(pixel *vid, int x, int y, int c, int r, int g, int b, int a)
{
	if (c < 0 || c >= 0x80)
		return x;
	int bn = 0, ba = 0;
	unsigned char *rp = font_data + font_ptrs[c];
	signed char w = *(rp++);
	unsigned char flags = *(rp++);
	signed char t = (flags&0x4) ? -(flags&0x3) : flags&0x3;
	signed char l = (flags&0x20) ? -((flags>>3)&0x3) : (flags>>3)&0x3;
	flags >>= 6;
	if (flags&0x2)
	{
		/*a = *(rp++);
		r = *(rp++);
		g = *(rp++);
		b = *(rp++);*/
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
			{
			addpixel(vid, x+i+l, y+j+t, r, g, b, ((ba&3)*a)/3);
			}
			ba >>= 2;
			bn -= 2;
		}
	return x + (flags&0x1 ? 0 : w);
}

int drawtext(pixel *vid, int x, int y, const char *s, int r, int g, int b, int a, bool noColor)
{
	if (FT_IsReady() && FT_HasUnicode(s))
		return FT_DrawText(vid, x, y, FT_ConvertIcons(std::string(s)).c_str(), r, g, b, a, noColor);

	int sx = x;
	bool highlight = false, underline = false;
	int oR = r, oG = g, oB = b;
	for (; *s; s++)
	{
		char c = *s;
		if (c == '\n' || c == '\r')
		{
			x = sx;
			y += FONT_H+2;
			if (highlight && (s[1] == '\n' || s[1] == '\r' || (s[1] == '\x01' && (s[2] == '\n' || s[2] == '\r'))))
			{
				fillrect(vid, x-1, y-3, font_data[font_ptrs[' ']]+1, FONT_H+3, 0, 0, 255, 127);
			}
		}
		else if (c == '\x0F')
		{
			if (!s[1] || !s[2] || !s[3])
				break;
			if (!noColor)
			{
				oR = r;
				oG = g;
				oB = b;
				r = (unsigned char)s[1];
				g = (unsigned char)s[2];
				b = (unsigned char)s[3];
			}
			s += 3;
		}
		else if (c == '\x0E')
		{
			r = oR;
			g = oG;
			b = oB;
		}
		else if (c == '\x01')
		{
			highlight = !highlight;
		}
		else if (c == '\b')
		{
			if (!noColor)
			{
				switch (s[1])
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
			}
			s++;
		}
		else
		{
			if (c == '\xEE')
			{
				char convertedC = Format::ConvertFontIcon(s, 0);
				if (convertedC != 0)
				{
					c = convertedC;
					s += 2;
				}
			}
			if (highlight)
			{
				fillrect(vid, x-1, y-3, font_data[font_ptrs[' ']]+1, FONT_H+3, 0, 0, 255, 127);
			}
			int newX = drawchar(vid, x, y, (unsigned char)c, r, g, b, a);
			if (underline)
			{
				blend_line(vid, x, y + FONT_H, newX - 1, y + FONT_H, r, g, b, a);
			}
			x = newX;
		}
	}
	return x;
}

int drawhighlight(pixel *vid, int x, int y, const char *s)
{
	if (FT_IsReady() && FT_HasUnicode(s))
		return FT_DrawText(vid, x, y, FT_ConvertIcons(std::string(s)).c_str(), 255, 255, 255, 255, true);

	int sx = x;
	for (; *s; s++)
	{
		if (*s == '\n')
		{
			x = sx;
			y += FONT_H+2;
		}
		else if (*s == '\x0F')
		{
			s += 3;
		}
		else if (*s == '\x0E')
		{
			
		}
		else if (*s == '\x01')
		{
			
		}
		else if (*s == '\b')
		{
			s++;
		}
		else
		{
			unsigned char uc = *(unsigned char *)s;
			if (uc >= 0x80) continue;
			int width = font_data[font_ptrs[uc]];
			fillrect(vid, x-1, y-3, width+1, FONT_H+3, 0, 0, 255, 127);
			x += width;
		}
	}
	return x;
}

//Draw text with an outline
int drawtext_outline(pixel *vid, int x, int y, const char *s, int r, int g, int b, int a, int outr, int outg, int outb, int outa)
{
	drawtext(vid, x-1, y-1, s, outr, outg, outb, outa, true);
	drawtext(vid, x+1, y+1, s, outr, outg, outb, outa, true);
	
	drawtext(vid, x-1, y+1, s, outr, outg, outb, outa, true);
	drawtext(vid, x+1, y-1, s, outr, outg, outb, outa, true);
	
	return drawtext(vid, x, y, s, r, g, b, a);
}
int drawtextwrap(pixel *vid, int x, int y, int w, int h, const char *s, int r, int g, int b, int a)
{
	if (FT_IsReady() && FT_HasUnicode(s))
		return FT_DrawText(vid, x, y, FT_ConvertIcons(std::string(s)).c_str(), r, g, b, a);

	int sx = x;
	int rh = 12;
	int cw = x;
	int wordlen;
	int charspace;
	int invert = 0;
	int oR = r, oG = g, oB = b;
	bool underline = false;
	if (FT_IsReady() && FT_HasUnicode(s))
		return FT_DrawText(vid, x, y, FT_ConvertIcons(std::string(s)).c_str(), r, g, b, a);
	while (*s)
	{
		wordlen = strcspn(s," .,!?\n");
		charspace = textwidthx((char *)s, w-(x-cw));
		if (charspace<wordlen && wordlen && w-(x-cw)<w/3)
		{
			x = sx;
			y += FONT_H+2;
			rh += FONT_H+2;
		}
		if ((h > 0 && rh > h) || (h < 0 && rh > YRES+MENUSIZE-110)) // the second part is hacky, since this will only be used for comments anyway
			break;
		for (; *s && --wordlen>=-1; s++)
		{
			if (*s == '\n')
			{
				x = sx;
				y += FONT_H+2;
				rh += FONT_H+2;
			}
			else if (*s == '\x0F')
			{
				if(!s[1] || !s[2] || !s[3])
					goto textwrapend;
				oR = r;
				oG = g;
				oB = b;
				r = (unsigned char)s[1];
				g = (unsigned char)s[2];
				b = (unsigned char)s[3];
				s += 3;
			}
			else if (*s == '\x0E')
			{
				r = oR;
				g = oG;
				b = oB;
			}
			else if (*s == '\x01')
			{
				invert = !invert;
				r = 255-r;
				g = 255-g;
				b = 255-b;
			}
			else if (*s == '\b')
			{
				bool colorCode = true;
				switch (s[1])
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
				case 'U':
					underline = !underline;
					colorCode = false;
					break;
				default:
					colorCode = false;
					break;
				}
				if (invert && colorCode)
				{
					r = 255-r;
					g = 255-g;
					b = 255-b;
				}
				s++;
			}
			else
			{

				if (x-cw>=w)
				{
					x = sx;
					y+=FONT_H+2;
					rh+=FONT_H+2;
					if (*s==' ')
						continue;
				}
				if ((h > 0 && rh > h) || (h < 0 && rh > YRES+MENUSIZE-110)) // the second part is hacky, since this will only be used for comments anyway
					goto textwrapend;
				int newX;
				if (rh + h < 0)
					newX = drawchar(vid, x, y, *(unsigned char *)s, 0, 0, 0, 0);
				else
					newX = drawchar(vid, x, y, *(unsigned char *)s, r, g, b, a);
				if (underline)
					blend_line(vid, x, y + FONT_H, newX - 1, y + FONT_H, r, g, b, a);
				x = newX;
			}
		}
	}

textwrapend:
	return rh;
}

int drawhighlightwrap(pixel *vid, int x, int y, int w, int h, const char *s, int highlightstart, int highlightlength)
{
	int sx = x;
	int rh = 12;
	int cw = x;
	int wordlen;
	int charspace;
	int num = 0;
	while (*s)
	{
		wordlen = strcspn(s," .,!?\n");
		charspace = textwidthx((char *)s, w-(x-cw));
		if (charspace<wordlen && wordlen && w-(x-cw)<w/3)
		{
			x = sx;
			y += FONT_H+2;
			rh += FONT_H+2;
		}
		if ((h > 0 && rh > h) || (h < 0 && rh > YRES+MENUSIZE-110)) // the second part is hacky, since this will only be used for comments anyway
			break;
		for (; *s && --wordlen>=-1; s++)
		{
			if (*s == '\n')
			{
				x = sx;
				y += FONT_H+2;
			}
			else if (*s == '\x0F')
			{
				s += 3;
				num += 4;
			}
			else if (*s == '\x0E')
			{
				num++;
			}
			else if (*s == '\x01')
			{
				num++;
			}
			else if (*s == '\b')
			{
				s++;
				num += 2;
			}
			else
			{
				unsigned char uc = *(unsigned char *)s;
				if (uc >= 0x80) { num++; continue; }
				int width = font_data[font_ptrs[uc]];
				if (x-cw>=w)
				{
					x = sx;
					y+=FONT_H+2;
					rh+=FONT_H+2;
					if (*s==' ')
					{
						num++;
						continue;
					}
				}
				if ((h > 0 && rh > h) || (h < 0 && rh > YRES+MENUSIZE-110)) // the second part is hacky, since this will only be used for comments anyway
					goto highlightwrapend;
				if (num >= highlightstart && num < highlightstart + highlightlength && rh + h >= 0)
					fillrect(vid, x-1, y-3, width+1, FONT_H+3, 0, 0, 255, 127);
				x += width;
				num++;
			}
		}
	}

highlightwrapend:
	return rh;
}

//draws a rectange, (x,y) are the top left coords.
void drawrect(pixel *vid, int x, int y, int w, int h, int r, int g, int b, int a)
{
	int i;
	for (i=0; i<=w; i++)
	{
		drawpixel(vid, x+i, y, r, g, b, a);
		drawpixel(vid, x+i, y+h, r, g, b, a);
	}
	for (i=1; i<h; i++)
	{
		drawpixel(vid, x, y+i, r, g, b, a);
		drawpixel(vid, x+w, y+i, r, g, b, a);
	}
}

//draws a rectangle and fills it in as well.
void fillrect(pixel *vid, int x, int y, int w, int h, int r, int g, int b, int a)
{
	int i,j;
	for (j=1; j<h; j++)
		for (i=1; i<w; i++)
			drawpixel(vid, x+i, y+j, r, g, b, a);
}

void drawcircle(pixel* vid, int x, int y, int rx, int ry, int r, int g, int b, int a)
{
	int tempy = y, i, j, oldy;
	if (!rx)
	{
		for (j = -ry; j <= ry; j++)
			drawpixel(vid, x, y+j, r, g, b, a);
		return;
	}
	for (i = x - rx; i <= x; i++)
	{
		oldy = tempy;
		while (pow(i-x, 2.0)*pow(ry, 2.0)+pow(tempy-y, 2.0)*pow(rx, 2.0) <= pow(rx, 2.0)*pow(ry, 2.0))
			tempy = tempy - 1;
		tempy = tempy + 1;
		if (oldy != tempy)
			oldy--;
		for (j = tempy; j <= oldy; j++)
		{
			int i2 = 2*x-i, j2 = 2*y-j;
			drawpixel(vid, i, j, r, g, b, a);
			if (i2 != i)
				drawpixel(vid, i2, j, r, g, b, a);
			if (j2 != j)
				drawpixel(vid, i, j2, r, g, b, a);
			if (i2 != i && j2 != j)
				drawpixel(vid, i2, j2, r, g, b, a);
		}
	}
}

void fillcircle(pixel* vid, int x, int y, int rx, int ry, int r, int g, int b, int a)
{
	int tempy = y, i, j, jmax;
	if (!rx)
	{
		for (j = -ry; j <= ry; j++)
			drawpixel(vid, x, y+j, r, g, b, a);
		return;
	}
	for (i = x - rx; i <= x; i++)
	{
		while (pow(i-x, 2.0)*pow(ry, 2.0)+pow(tempy-y, 2.0)*pow(rx, 2.0) <= pow(rx, 2.0)*pow(ry, 2.0))
			tempy = tempy - 1;
		tempy = tempy + 1;
		jmax = 2*y - tempy;
		for (j = tempy; j <= jmax; j++)
		{
			drawpixel(vid, i, j, r, g, b, a);
			if (i != x)
				drawpixel(vid, 2*x-i, j, r, g, b, a);
		}
	}
}

void clearrect(pixel *vid, int x, int y, int w, int h)
{
	int i;

	if (x+w > XRES+BARSIZE) w = XRES+BARSIZE-x;
	if (y+h > YRES+MENUSIZE) h = YRES+MENUSIZE-y;
	if (x<0)
	{
		w += x;
		x = 0;
	}
	if (y<0)
	{
		h += y;
		y = 0;
	}
	if (w<0 || h<0)
		return;

	for (i=0; i<h; i++)
		memset(vid+(x+(XRES+BARSIZE)*(y+i)), 0, PIXELSIZE*w);
}
//draws a line of dots, where h is the height. (why is this even here)
void drawdots(pixel *vid, int x, int y, int h, int r, int g, int b, int a)
{
	int i;
	for (i=0; i<=h; i+=2)
		drawpixel(vid, x, y+i, r, g, b, a);
}

int charwidth(unsigned char c)
{
	if (c >= 0x80)
		return 0;
	short font_ptr = font_ptrs[static_cast<int>(c)];
	return (font_data[font_ptr+1]&0x40) ? 0 : font_data[font_ptr];
}

int textwidth(const char *s)
{
	if (FT_IsReady() && FT_HasUnicode(s))
		return FT_TextWidth(FT_ConvertIcons(std::string(s)).c_str());

	int x = 0, maxX = 0;
	for (; *s; s++)
		if (*s == '\n')
		{
			x = 0;
		}
		else if (*s == '\x0F')
		{
			s += 3;
		}
		else if (*s == '\x0E')
		{
		}
		else if (*s == '\x01')
		{
		}
		else if (*s == '\b')
		{
			s++;
		}
		else
		{
			x += charwidth(*s);
			if (x > maxX)
				maxX = x;
		}
	return maxX-1;
}

int drawtextmax(pixel *vid, int x, int y, int w, const char *s, int r, int g, int b, int a)
{
	if (FT_IsReady() && FT_HasUnicode(s))
		return FT_DrawText(vid, x, y, FT_ConvertIcons(std::string(s)).c_str(), r, g, b, a);

	int i;
	w += x-5;
	for (; *s; s++)
	{
		if (x+charwidth(*s)>=w && x+textwidth(s)>=w+5)
			break;
		x = drawchar(vid, x, y, *(unsigned char *)s, r, g, b, a);
	}
	if (*s)
		for (i=0; i<3; i++)
			x = drawchar(vid, x, y, '.', r, g, b, a);
	return x;
}

int textnwidth(char *s, int n)
{
	int x = 0;
	for (; *s; s++)
	{
		if (!n)
			break;
		if (*s == '\x0F')
		{
			s += 3;
			n = std::min(1,n-3);
		}
		else if (*s == '\x0E')
		{
		}
		else if (*s == '\x01')
		{
		}
		else if (*s == '\b')
		{
			s++;
			if (n > 1)
				n--;
		}
		else
			x += charwidth(*s);
		n--;
	}
	return x-1;
}
void textnpos(char *s, int n, int w, int *cx, int *cy)
{
	int x = 0;
	int y = 0;
	int wordlen, charspace;
	while (*s&&n)
	{
		wordlen = strcspn(s," .,!?\n");
		charspace = textwidthx(s, w-x);
		if (charspace<wordlen && wordlen && w-x<w/3)
		{
			x = 0;
			y += FONT_H+2;
		}
		for (; *s && --wordlen>=-1; s++)
		{
			if (!n) {
				break;
			}
			if (*s == '\n')
			{
				x = 0;
				y += FONT_H+2;
				continue;
			}
			else if (*s == '\x0F')
			{
				s += 3;
				n = std::min(1,n-3);
			}
			else if (*s == '\x0E')
			{
			}
			else if (*s == '\x01')
			{
			}
			else if (*s == '\b')
			{
				s++;
				if (n > 1)
					n--;
			}
			else
				x += charwidth(*s);
			if (x>=w)
			{
				x = 0;
				y += FONT_H+2;
				if (*(s+1)==' ')
					x -= charwidth(' ');
			}
			n--;
		}
	}
	*cx = x-1;
	*cy = y;
}

int textwidthx(char *s, int w)
{
	int x=0,n=0,cw;
	for (; *s; s++)
	{
		if (*s == '\x0F')
		{
			s += 4;
			n += 4;
			if (!*s)
				break;
		}
		else if (*s == '\x0E')
		{
		}
		else if (*s == '\x01')
		{
		}
		else if (*s == '\b')
		{
			s+=2;
			n+=2;
			if (!*s)
				break;
		}
		cw = charwidth(*s);
		if (x+(cw/2) >= w)
			break;
		x += cw;
		n++;
	}
	return n;
}
void textsize(char * s, int *width, int *height)
{
	if (FT_IsReady() && FT_HasUnicode(s))
	{
		int w = FT_TextWidth(FT_ConvertIcons(std::string(s)).c_str());
		int h = FT_TextHeight();
		// Count newlines
		for (const char *p = s; *p; p++)
			if (*p == '\n') h += FT_TextHeight() + 2;
		*width = w;
		*height = h;
		return;
	}

	int cHeight = FONT_H, cWidth = 0, lWidth = 0;
	if(!strlen(s))
	{
		*width = 0;
		*height = FONT_H;
		return;
	}

	for (; *s; s++)
	{
		if (*s == '\n')
		{
			cWidth = 0;
			cHeight += FONT_H+2;
		}
		else if (*s == '\x0F')
		{
			if(!s[1] || !s[2] || !s[1]) break;
			s+=3;
		}
		else if (*s == '\b')
		{
			if(!s[1]) break;
			s++;
		}
		else
		{
			cWidth += charwidth(*s);
			if (cWidth>lWidth)
				lWidth = cWidth;
		}
	}
	*width = lWidth;
	*height = cHeight;
}
int textposxy(char *s, int width, int w, int h)
{
	if (FT_IsReady() && FT_HasUnicode(s))
		return strlen(s); // fallback: return full length to show all

	int x=0,y=0,n=0,cw, wordlen, charspace;
	while (*s)
	{
		wordlen = strcspn(s," .,!?\n");
		charspace = textwidthx(s, width-x);
		if (charspace<wordlen && wordlen && width-x<width/3)
		{
			x = 0;
			y += FONT_H+2;
		}
		for (; *s && --wordlen>=-1; s++)
		{
			if (*s == '\n')
			{
				x = 0;
				y += FONT_H+2;
				continue;
			}
			else if (*s == '\x0F')
			{
				s += 4;
				n += 4;
				if (!*s)
					return n;
			}
			else if (*s == '\x0E')
			{
			}
			else if (*s == '\x01')
			{
			}
			else if (*s == '\b')
			{
				s+=2;
				n+=2;
				if (!*s)
					break;
			}
			cw = charwidth(*s);
			if ((x+(cw/2) >= w && y+6 >= h)||(y+6 >= h+FONT_H+2))
				return n++;
			x += cw;
			if (x>=width) {
				x = 0;
				y += FONT_H+2;
				if (*(s+1)==' ')
					x -= charwidth(' ');
			}
			n++;
		}
	}
	return n;
}
int textwrapheight(char *s, int width)
{
	if (FT_IsReady() && FT_HasUnicode(s))
	{
		int tw = FT_TextWidth(FT_ConvertIcons(std::string(s)).c_str());
		int th = FT_TextHeight() + 2;
		int lines = (tw > 0 && width > 0) ? (tw / width + 1) : 1;
		return th * lines;
	}

	int x=0, height=FONT_H+2, cw = 0;
	int wordlen;
	int charspace;
	while (*s)
	{
		wordlen = strcspn(s," .,!?\n");
		charspace = textwidthx(s, width-x);
		if (charspace<wordlen && wordlen && width-x<width/3)
		{
			x = 0;
			height += FONT_H+2;
		}
		for (; *s && --wordlen>=-1; s++)
		{
			if (*s == '\n')
			{
				x = 0;
				height += FONT_H+2;
			}
			else if (*s == '\x0F')
			{
				s += 3;
			}
			else if (*s == '\x0E')
			{
			}
			else if (*s == '\x01')
			{
			}
			else if (*s == '\b')
			{
				s++;
			}
			else
			{
				if (x-cw>=width)
				{
					x = 0;
					height += FONT_H+2;
					if (*s==' ')
						continue;
				}
				cw = charwidth(*s);
				x += cw;
			}
		}
	}
	return height;
}

//the most used function for drawing a pixel, because it has OpenGL support, which is not fully implemented.
void blendpixel(pixel *vid, int x, int y, int r, int g, int b, int a)
{
#ifdef PIXALPHA
	pixel t;
	if (x < clip_rect_x1 || y < clip_rect_y1 || x >= clip_rect_x2 || y >= clip_rect_y2)
		return;
	if (a!=255)
	{
		t = vid[y*(XRES+BARSIZE)+x];
		r = (a*r + (255-a)*PIXR(t)) >> 8;
		g = (a*g + (255-a)*PIXG(t)) >> 8;
		b = (a*b + (255-a)*PIXB(t)) >> 8;
		a = a > PIXA(t) ? a : PIXA(t);
	}
	vid[y*(XRES+BARSIZE)+x] = PIXRGBA(r,g,b,a);
#else
	pixel t;
	if (x < clip_rect_x1 || y < clip_rect_y1 || x >= clip_rect_x2 || y >= clip_rect_y2 || a == 0)
		return;
	if (a!=255)
	{
		t = vid[y*(XRES+BARSIZE)+x];
		r = (a*r + (255-a)*PIXR(t)) >> 8;
		g = (a*g + (255-a)*PIXG(t)) >> 8;
		b = (a*b + (255-a)*PIXB(t)) >> 8;
	}
	vid[y*(XRES+BARSIZE)+x] = PIXRGB(r,g,b);
#endif
}

void draw_icon(pixel *vid_buf, int x, int y, char ch, int flag)
{
	char t[2];
	t[0] = ch;
	t[1] = 0;
	if (flag)
	{
		fillrect(vid_buf, x-1, y-1, 17, 17, 255, 255, 255, 255);
		drawtext(vid_buf, x+3, y+2, t, 0, 0, 0, 255);
	}
	else
	{
		drawrect(vid_buf, x, y, 15, 15, 255, 255, 255, 255);
		drawtext(vid_buf, x+3, y+2, t, 255, 255, 255, 255);
	}
}

pixel HeatToColor(float temp)
{
	int caddress = (int)restrict_flt((int)(restrict_flt(temp, 0.0f, (float)MAX_TEMP+(-MIN_TEMP)) / ((MAX_TEMP+(-MIN_TEMP))/1024) ) * 3.0f, 0.0f, (1024.0f * 3) - 3);
	return COLARGB(255, (int)((unsigned char)color_data[caddress] * 0.7f), (int)((unsigned char)color_data[caddress + 1] * 0.7f), (int)((unsigned char)color_data[caddress + 2] * 0.7f));
}

pixel PressureToColor(float pres)
{
	if (pres > 0.0f)
		return COLARGB(255, clamp_flt(pres, 0.0f, 8.0f), 0, 0); //positive pressure is red!
	else
		return COLARGB(255, 0, 0, clamp_flt(-pres, 0.0f, 8.0f)); //negative pressure is blue!
}

pixel VelocityToColor(float vx, float vy, float pres)
{
	return COLARGB(
		255,
		clamp_flt(fabsf(vx), 0.0f, 8.0f), // vx adds red
		clamp_flt(pres, 0.0f, 8.0f), // pressure adds green
		clamp_flt(fabsf(vy), 0.0f, 8.0f) // vy adds blue
	);
}

void draw_air(pixel *vid, Simulation * sim)
{
	pixel c;
	unsigned int display_mode = Renderer::Ref().GetDisplayMode();
	for (int y = 0; y < YRES/CELL; y++)
		for (int x = 0; x < XRES/CELL; x++)
		{
			if (display_mode & DISPLAY_AIRP)
			{
				c = PressureToColor(sim->air->pv[y][x]);
				c = PIXPACK(c);
			}
			else if (display_mode & DISPLAY_AIRV)
			{
				c = VelocityToColor(sim->air->vx[y][x], sim->air->vy[y][x], sim->air->pv[y][x]);
				c = PIXPACK(c);
			}
			else if (display_mode & DISPLAY_AIRH)
			{
				if (!aheat_enable)
					c = 0;
				else
				{
					float ttemp = sim->air->hv[y][x]+(-MIN_TEMP);
					c = HeatToColor(ttemp);
					c = PIXPACK(c);
				}
			}
			else if (display_mode & DISPLAY_AIRC)
			{
				int r;
				int g;
				int b;
				// velocity adds grey
				r = clamp_flt(fabsf(sim->air->vx[y][x]), 0.0f, 24.0f) + clamp_flt(fabsf(sim->air->vy[y][x]), 0.0f, 20.0f);
				g = clamp_flt(fabsf(sim->air->vx[y][x]), 0.0f, 20.0f) + clamp_flt(fabsf(sim->air->vy[y][x]), 0.0f, 24.0f);
				b = clamp_flt(fabsf(sim->air->vx[y][x]), 0.0f, 24.0f) + clamp_flt(fabsf(sim->air->vy[y][x]), 0.0f, 20.0f);
				if (sim->air->pv[y][x] > 0.0f)
				{
					r += clamp_flt(sim->air->pv[y][x], 0.0f, 16.0f);//pressure adds red!
					if (r>255)
						r=255;
					if (g>255)
						g=255;
					if (b>255)
						b=255;
					c  = PIXRGB(r, g, b);
				}
				else
				{
					b += clamp_flt(-sim->air->pv[y][x], 0.0f, 16.0f);//pressure adds blue!
					if (r>255)
						r=255;
					if (g>255)
						g=255;
					if (b>255)
						b=255;
					c  = PIXRGB(r, g, b);
				}
			}
			else if (display_mode & DISPLAY_AIRW)
			{
				auto w = 4 * Air::vorticity(sim->air, y, x);
				if (w > 0.0f)
					c = PIXRGB(clamp_flt(w, 0.0f, 8.0f), 0, 0); //positive vorticity is red
				else
					c = PIXRGB(0, 0, clamp_flt(-w, 0.0f, 8.0f)); //negative vorticity is blue
			}
			if (finding && !(finding & 0x8))
			{
				c = PIXRGB(PIXR(c)/10,PIXG(c)/10,PIXB(c)/10);
			}
			// Draws the colors
			for (int j = 0; j < CELL; j++)
				for (int i = 0; i < CELL; i++)
					vid[(x*CELL+i) + (y*CELL+j)*(XRES+BARSIZE)] = c;
		}
}

void draw_grav_zones(pixel * vid)
{
	int x, y, i, j;
	for (y=0; y<YRES/CELL; y++)
	{
		for (x=0; x<XRES/CELL; x++)
		{
			if (globalSim->grav->gravmask[y*(XRES/CELL)+x])
			{
				for (j=0; j<CELL; j++)//draws the colors
					for (i=0; i<CELL; i++)
						if (i == j)
							drawpixel(vid, x*CELL+i, y*CELL+j, 255, 200, 0, 120);
						else 
							drawpixel(vid, x*CELL+i, y*CELL+j, 32, 32, 32, 120);
			}
		}
	}
}

void draw_grav(pixel *vid)
{
	int x, y, i, ca;
	float nx, ny, dist;

	for (y=0; y<YRES/CELL; y++)
	{
		for (x=0; x<XRES/CELL; x++)
		{
			ca = y*(XRES/CELL)+x;
			if (fabsf(globalSim->grav->gravx[ca]) <= 0.001f && fabsf(globalSim->grav->gravy[ca]) <= 0.001f)
				continue;
			nx = (float)x*CELL;
			ny = (float)y*CELL;
			dist = fabsf(globalSim->grav->gravy[ca])+fabsf(globalSim->grav->gravx[ca]);
			for (i = 0; i < 4; i++)
			{
				nx -= globalSim->grav->gravx[ca] * 0.5f;
				ny -= globalSim->grav->gravy[ca] * 0.5f;
				addpixel(vid, (int)(nx+0.5f), (int)(ny+0.5f), 255, 255, 255, (int)(dist*20.0f));
			}
		}
	}
}

void draw_line(pixel *vid, int x1, int y1, int x2, int y2, int r, int g, int b, int screenwidth)  //Draws a line
{
	int dx, dy, i, sx, sy, check, e, x, y;

	dx = std::abs(x1-x2);
	dy = std::abs(y1-y2);
	sx = isign<int>(x2-x1);
	sy = isign<int>(y2-y1);
	x = x1;
	y = y1;
	check = 0;

	if (dy > dx)
	{
		dx = dx + dy;
		dy = dx - dy;
		dx = dx - dy;
		check = 1;
	}

	e = (dy<<2) - dx;
	for (i = 0; i <= dx; i++)
	{
		if (x>=0 && y>=0 && x<screenwidth && y<YRES+MENUSIZE)
			vid[x + y*screenwidth] = PIXRGB(r, g, b);
		if (e >= 0)
		{
			if (check == 1)
				x = x + sx;
			else
				y = y + sy;
			e = e - (dx<<2);
		}
		if (check == 1)
			y = y + sy;
		else
			x = x + sx;
		e = e + (dy<<2);
	}
}

//adds color to a pixel, does not overwrite.
void addpixel(pixel *vid, int x, int y, int r, int g, int b, int a)
{
	pixel t;
	if (x < clip_rect_x1 || y < clip_rect_y1 || x >= clip_rect_x2 || y >= clip_rect_y2)
		return;
	t = vid[y*(XRES+BARSIZE)+x];
	r = (a*r + 255*PIXR(t)) >> 8;
	g = (a*g + 255*PIXG(t)) >> 8;
	b = (a*b + 255*PIXB(t)) >> 8;
	if (r>255)
		r = 255;
	if (g>255)
		g = 255;
	if (b>255)
		b = 255;
	vid[y*(XRES+BARSIZE)+x] = PIXRGB(r,g,b);
}

//draws one of two colors, so that it is always clearly visible
void xor_pixel(int x, int y, pixel *vid)
{
	int c;
	if (x < clip_rect_x1 || y < clip_rect_y1 || x >= clip_rect_x2 || y >= clip_rect_y2)
		return;
	c = vid[y*(XRES+BARSIZE)+x];
	c = PIXB(c) + 3*PIXG(c) + 2*PIXR(c);
	if (c<512)
		vid[y*(XRES+BARSIZE)+x] = PIXPACK(0xC0C0C0);
	else
		vid[y*(XRES+BARSIZE)+x] = PIXPACK(0x404040);
}

//same as xor_pixel, but draws a line of it
void xor_line(int x1, int y1, int x2, int y2, pixel *vid)
{
	int cp=abs(y2-y1)>abs(x2-x1), x, y, dx, dy, sy;
	float e, de;
	if (cp)
	{
		y = x1;
		x1 = y1;
		y1 = y;
		y = x2;
		x2 = y2;
		y2 = y;
	}
	if (x1 > x2)
	{
		y = x1;
		x1 = x2;
		x2 = y;
		y = y1;
		y1 = y2;
		y2 = y;
	}
	dx = x2 - x1;
	dy = abs(y2 - y1);
	e = 0.0f;
	if (dx)
		de = dy/(float)dx;
	else
		de = 0.0f;
	y = y1;
	sy = (y1<y2) ? 1 : -1;
	for (x=x1; x<=x2; x++)
	{
		if (cp)
			xor_pixel(y, x, vid);
		else
			xor_pixel(x, y, vid);
		e += de;
		if (e >= 0.5f)
		{
			y += sy;
			e -= 1.0f;
		}
	}
}

//same as blend_pixel, but draws a line of it
void blend_line(pixel *vid, int x1, int y1, int x2, int y2, int r, int g, int b, int a)
{
	int cp=abs(y2-y1)>abs(x2-x1), x, y, dx, dy, sy;
	float e, de;
	if (cp)
	{
		y = x1;
		x1 = y1;
		y1 = y;
		y = x2;
		x2 = y2;
		y2 = y;
	}
	if (x1 > x2)
	{
		y = x1;
		x1 = x2;
		x2 = y;
		y = y1;
		y1 = y2;
		y2 = y;
	}
	dx = x2 - x1;
	dy = abs(y2 - y1);
	e = 0.0f;
	if (dx)
		de = dy/(float)dx;
	else
		de = 0.0f;
	y = y1;
	sy = (y1<y2) ? 1 : -1;
	for (x=x1; x<=x2; x++)
	{
		if (cp)
			blendpixel(vid, y, x, r, g, b, a);
		else
			blendpixel(vid, x, y, r, g, b, a);
		e += de;
		if (e >= 0.5f)
		{
			y += sy;
			e -= 1.0f;
		}
	}
}

//same as xor_pixel, but draws a rectangle
void xor_rect(pixel *vid, int x, int y, int w, int h)
{
	int i;
	for (i=0; i<w; i+=2)
	{
		xor_pixel(x+i, y, vid);
	}
	if (h != 1)
	{
		if (h%2 == 1) i = 2;
		else i = 1;
		for (; i<w; i+=2)
		{
			xor_pixel(x+i, y+h-1, vid);
		}
	}

	for (i=2; i<h; i+=2)
	{
		xor_pixel(x, y+i, vid);
	}
	if (w != 1)
	{
		if (w%2 == 1) i = 2;
		else i = 1;
		for (; i<h-1; i+=2)
		{
			xor_pixel(x+w-1, y+i, vid);
		}
	}
}

void draw_other(pixel *vid, Simulation * sim) // EMP effect
{
	if (!sys_pause || framerender)
		static_cast<EMP_ElementDataContainer&>(*sim->elementData[PT_EMP]).Deactivate();
	if (!(Renderer::Ref().GetRenderMode() & EFFECT)) // not in nothing mode
		return;

	int emp_decor = static_cast<EMP_ElementDataContainer&>(*sim->elementData[PT_EMP]).emp_decor;
	if (emp_decor)
	{
		int r=(int)(emp_decor*2.5), g=(int)(100+emp_decor*1.5), b=255;
		int a=(int)((1.0*emp_decor/110)*255);
		if (r>255) r=255;
		if (g>255) g=255;
		if (b>255) g=255;
		if (a>255) a=255;
		for (int j = 0; j < YRES; j++)
			for (int i = 0; i < XRES; i++)
			{
				drawpixel(vid, i, j, r, g, b, a);
			}
	}
}

void prepare_graphicscache()
{
	graphicscache = (gcache_item*)malloc(sizeof(gcache_item)*PT_NUM);
	memset(graphicscache, 0, sizeof(gcache_item)*PT_NUM);
}

void render_parts(pixel *vid, Simulation * sim, Point mousePos)
{
	int deca, decr, decg, decb, cola, colr, colg, colb, firea, firer = 0, fireg = 0, fireb = 0, pixel_mode, q, t, nx, ny, x, y, caddress;
	int orbd[4] = {0, 0, 0, 0}, orbl[4] = {0, 0, 0, 0};
	float gradv, flicker;
	unsigned int render_mode = Renderer::Ref().GetRenderMode();
	unsigned int display_mode = Renderer::Ref().GetDisplayMode();
	unsigned int color_mode = Renderer::Ref().GetColorMode();
	int drawing_budget = 1000000; //Serves as an upper bound for costly effects such as SPARK, FLARE and LFLARE

	if (GRID_MODE)//draws the grid
	{
		for (ny=0; ny<YRES; ny++)
			for (nx=0; nx<XRES; nx++)
			{
				if (ny%(4*GRID_MODE) == 0)
					blendpixel(vid, nx, ny, 100, 100, 100, 80);
				if (nx%(4*GRID_MODE) == 0 && ny%(4*GRID_MODE) != 0)
					blendpixel(vid, nx, ny, 100, 100, 100, 80);
			}
	}
	foundParticles = 0;
	for(int i = 0; i <= sim->parts_lastActiveIndex; i++) {
		if (parts[i].type) {
			t = parts[i].type;
#ifndef NOMOD
			if (t == PT_PINV && parts[i].tmp2 && ID(parts[i].tmp2) < i)
				continue;
#endif

			nx = (int)(parts[i].x+0.5f);
			ny = (int)(parts[i].y+0.5f);
			if (nx < 0 || nx > XRES || ny < 0 || ny > YRES)
				continue;
#ifndef NOMOD
			if (TYP(pmap[ny][nx]) == PT_PINV)
				parts[ID(pmap[ny][nx])].tmp2 = PMAP(i, t);
#endif

			if (photons[ny][nx] && !(sim->elements[t].Properties & TYPE_ENERGY) && t!=PT_STKM && t!=PT_STKM2 && t!=PT_FIGH)
				continue;
				
			//Defaults
			pixel_mode = 0 | PMODE_FLAT;
			cola = COLA(sim->elements[t].Colour); // always 255
			colr = COLR(sim->elements[t].Colour);
			colg = COLG(sim->elements[t].Colour);
			colb = COLB(sim->elements[t].Colour);
			firea = 0;
			
			deca = COLA(parts[i].dcolour);
			decr = COLR(parts[i].dcolour);
			decg = COLG(parts[i].dcolour);
			decb = COLB(parts[i].dcolour);
				
			/*if(display_mode == RENDER_NONE)
			{
				if(decorations_enable)
				{
					colr = (deca*decr + (255-deca)*colr) >> 8;
					colg = (deca*decg + (255-deca)*colg) >> 8;
					colb = (deca*decb + (255-deca)*colb) >> 8;
				}
				vid[ny*(XRES+BARSIZE)+nx] = PIXRGB(colr,colg,colb);
			}
			else*/
			{	
				if (graphicscache[t].isready)
				{
					pixel_mode = graphicscache[t].pixel_mode;
					cola = graphicscache[t].cola;
					colr = graphicscache[t].colr;
					colg = graphicscache[t].colg;
					colb = graphicscache[t].colb;
					firea = graphicscache[t].firea;
					firer = graphicscache[t].firer;
					fireg = graphicscache[t].fireg;
					fireb = graphicscache[t].fireb;
				}
				else if(!(color_mode & COLOR_BASC))	//Don't get special effects for BASIC colour mode
				{
					int (*graphicsFunc) (GRAPHICS_FUNC_ARGS) = sim->elements[t].Graphics;
					if (graphicsFunc == nullptr)
						graphicsFunc = graphics_DEFAULT;

					// That's a lot of args, a struct might be better
					if (graphicsFunc(sim, &(parts[i]), nx, ny, &pixel_mode, &cola, &colr, &colg, &colb, &firea, &firer, &fireg, &fireb))
					{
						graphicscache[t].isready = 1;
						graphicscache[t].pixel_mode = pixel_mode;
						graphicscache[t].cola = cola;
						graphicscache[t].colr = colr;
						graphicscache[t].colg = colg;
						graphicscache[t].colb = colb;
						graphicscache[t].firea = firea;
						graphicscache[t].firer = firer;
						graphicscache[t].fireg = fireg;
						graphicscache[t].fireb = fireb;
					}
				}
				if(sim->elements[t].Properties & PROP_HOT_GLOW && parts[i].temp > (sim->elements[t].HighTemperatureTransitionThreshold-800.0f))
				{
					int transitionTemp = sim->elements[t].HighTemperatureTransitionThreshold;
					gradv = M_PI / (transitionTemp + 800.0f);
					caddress = (int)(parts[i].temp > transitionTemp ? 800.0f : parts[i].temp - (transitionTemp - 800.0f));
					colr += (int)(sin(gradv*caddress) * 226);
					colg += (int)(sin(gradv*caddress*4.55 +3.14) * 34);
					colb += (int)(sin(gradv*caddress*2.22 +3.14) * 64);
				}
				
				if(pixel_mode & FIRE_ADD && !(render_mode & FIRE_ADD))
					pixel_mode |= PMODE_GLOW;
				if(pixel_mode & FIRE_BLEND && !(render_mode & FIRE_BLEND))
					pixel_mode |= PMODE_BLUR;
				if(pixel_mode & PMODE_BLUR && !(render_mode & PMODE_BLUR))
					pixel_mode |= PMODE_FLAT;
				if(pixel_mode & PMODE_GLOW && !(render_mode & PMODE_GLOW))
					pixel_mode |= PMODE_BLEND;
				if (render_mode & PMODE_BLOB)
					pixel_mode |= PMODE_BLOB;
					
				pixel_mode &= render_mode;
				
				//Alter colour based on display mode
				if(color_mode & COLOR_HEAT)
				{
					if (heatmode == 0)
						caddress = (int)restrict_flt((int)( restrict_flt((float)(parts[i].temp+(-MIN_TEMP)), 0.0f, MAX_TEMP+(-MIN_TEMP)) / ((MAX_TEMP+(-MIN_TEMP))/1024) ) *3.0f, 0.0f, (1024.0f*3)-3); //Not having that second (float) might be a bug, and is definetely needed if min&max temps are less than 1024 apart
					else
						caddress = (int)restrict_flt((int)( restrict_flt((float)(parts[i].temp+(-lowesttemp)), 0.0f, (float)highesttemp+(-lowesttemp)) / ((float)(highesttemp+(-lowesttemp))/1024) ) *3.0f, 0.0f, (1024.0f*3)-3);
					firea = 255;
					firer = colr = (unsigned char)color_data[caddress];
					fireg = colg = (unsigned char)color_data[caddress+1];
					fireb = colb = (unsigned char)color_data[caddress+2];
					cola = 255;
					if (pixel_mode & (FIREMODE | PMODE_GLOW))
						pixel_mode = (pixel_mode & ~(FIREMODE|PMODE_GLOW)) | PMODE_BLUR;
					else if ((pixel_mode & (PMODE_BLEND | PMODE_ADD)) == (PMODE_BLEND | PMODE_ADD))
						pixel_mode = (pixel_mode & ~(PMODE_BLEND|PMODE_ADD)) | PMODE_FLAT;
					else if (!pixel_mode)
						pixel_mode |= PMODE_FLAT;
				}
				else if(color_mode & COLOR_LIFE)
				{
					gradv = 0.4f;
					if (!(parts[i].life<5))
						q = (int)sqrtf((float)parts[i].life);
					else
						q = parts[i].life;
					colr = colg = colb = (int)(sin(gradv*q) * 100 + 128);
					cola = 255;
					if (pixel_mode & (FIREMODE | PMODE_GLOW))
						pixel_mode = (pixel_mode & ~(FIREMODE|PMODE_GLOW)) | PMODE_BLUR;
					else if ((pixel_mode & (PMODE_BLEND | PMODE_ADD)) == (PMODE_BLEND | PMODE_ADD))
						pixel_mode = (pixel_mode & ~(PMODE_BLEND|PMODE_ADD)) | PMODE_FLAT;
					else if (!pixel_mode)
						pixel_mode |= PMODE_FLAT;
				}
				else if (color_mode & COLOR_BASC)
				{
					colr = COLR(sim->elements[t].Colour);
					colg = COLG(sim->elements[t].Colour);
					colb = COLB(sim->elements[t].Colour);
					pixel_mode = PMODE_FLAT;
				}

				// Apply decoration color
				if (!(color_mode & ~COLOR_GRAD) && decorations_enable && deca)
				{
					deca++;
					if (!(pixel_mode & NO_DECO))
					{
						colr = (deca*decr + (256-deca)*colr) >> 8;
						colg = (deca*decg + (256-deca)*colg) >> 8;
						colb = (deca*decb + (256-deca)*colb) >> 8;
					}

					if (pixel_mode & DECO_FIRE)
					{
						firer = (deca*decr + (256-deca)*firer) >> 8;
						fireg = (deca*decg + (256-deca)*fireg) >> 8;
						fireb = (deca*decb + (256-deca)*fireb) >> 8;
					}
				}

				if (color_mode & COLOR_GRAD)
				{
					float frequency = 0.05f;
					int q = (int)parts[i].temp-40;
					colr = (int)(sin(frequency*q) * 16 + colr);
					colg = (int)(sin(frequency*q) * 16 + colg);
					colb = (int)(sin(frequency*q) * 16 + colb);
					if(pixel_mode & (FIREMODE | PMODE_GLOW)) pixel_mode = (pixel_mode & ~(FIREMODE|PMODE_GLOW)) | PMODE_BLUR;
				}

				//All colours are now set, check ranges
				if(colr>255) colr = 255;
				else if(colr<0) colr = 0;
				if(colg>255) colg = 255;
				else if(colg<0) colg = 0;
				if(colb>255) colb = 255;
				else if(colb<0) colb = 0;
				if(cola>255) cola = 255;
				else if(cola<0) cola = 0;

				if(firer>255) firer = 255;
				else if(firer<0) firer = 0;
				if(fireg>255) fireg = 255;
				else if(fireg<0) fireg = 0;
				if(fireb>255) fireb = 255;
				else if(fireb<0) fireb = 0;
				if(firea>255) firea = 255;
				else if(firea<0) firea = 0;

				if (finding && !(finding & 0x8))
				{
					if ((finding & 0x1) && find_matches(i, activeTools[0]))
					{
						colr = firer = 255;
						colg = colb = fireg = fireb = 0;
						cola = firea = 255;
						foundParticles++;
					}
					else if ((finding & 0x2) && find_matches(i, activeTools[1]))
					{
						colb = fireb = 255;
						colr = colg = firer = fireg = 0;
						cola = firea = 255;
						foundParticles++;
					}
					else if ((finding & 0x4) && find_matches(i, activeTools[2]))
					{
						colg = fireg = 255;
						colr = colb = firer = fireb = 0;
						cola = firea = 255;
						foundParticles++;
					}
					else
					{
						colr /= 10;
						colg /= 10;
						colb /= 10;
						firer /= 5;
						fireg /= 5;
						fireb /= 5;
					}
				}

				//Pixel rendering
				if (t==PT_SOAP) //pixel_mode & EFFECT_LINES, pointless to check if only soap has it ...
				{
					if ((parts[i].ctype&3) == 3 && parts[i].tmp >= 0 && parts[i].tmp < NPART)
						draw_line(vid, nx, ny, (int)(parts[parts[i].tmp].x+0.5f), (int)(parts[parts[i].tmp].y+0.5f), colr, colg, colb, XRES+BARSIZE);
				}
				if(pixel_mode & PSPEC_STICKMAN)
				{
					char buff[20];  //Buffer for HP
					int s;
					int legr, legg, legb;
					Stickman *cplayer;
					if (t == PT_STKM)
						cplayer = static_cast<STKM_ElementDataContainer&>(*sim->elementData[PT_STKM]).GetStickman1();
					else if (t == PT_STKM2)
						cplayer = static_cast<STKM_ElementDataContainer&>(*sim->elementData[PT_STKM]).GetStickman2();
					else if (t == PT_FIGH && parts[i].tmp >= 0 && parts[i].tmp < static_cast<FIGH_ElementDataContainer&>(*sim->elementData[PT_FIGH]).MaxFighters())
						cplayer = static_cast<FIGH_ElementDataContainer&>(*sim->elementData[PT_FIGH]).Get(parts[i].tmp);
					else
						continue;

					if (mousePos.X>nx-3 && mousePos.X<nx+3 && mousePos.Y<ny+3 && mousePos.Y>ny-3) //If mouse is in the head
					{
						sprintf(buff, "%3d", parts[i].life);  //Show HP
						drawtext(vid, mousePos.X-8-2*(parts[i].life<100)-2*(parts[i].life<10), mousePos.Y-12, buff, 255, 255, 255, 255);
					}

					if (color_mode!=COLOR_HEAT && !(finding & ~0x8))
					{
						if (!cplayer->fan && cplayer->elem > 0 && cplayer->elem < PT_NUM)
						{
							colr = COLR(sim->elements[cplayer->elem].Colour);
							colg = COLG(sim->elements[cplayer->elem].Colour);
							colb = COLB(sim->elements[cplayer->elem].Colour);
						}
						else
						{
							colr = 0x80;
							colg = 0x80;
							colb = 0xFF;
						}
					}
					s = XRES+BARSIZE;

					if (t==PT_STKM2)
					{
						legr = 100;
						legg = 100;
						legb = 255;
					}
					else
					{
						legr = 255;
						legg = 255;
						legb = 255;
					}

					if (color_mode==COLOR_HEAT || (finding & ~0x8))
					{
						legr = colr;
						legg = colg;
						legb = colb;
					}

					//head
					if(t==PT_FIGH)
					{
						draw_line(vid , nx, ny+2, nx+2, ny, colr, colg, colb, s);
						draw_line(vid , nx+2, ny, nx, ny-2, colr, colg, colb, s);
						draw_line(vid , nx, ny-2, nx-2, ny, colr, colg, colb, s);
						draw_line(vid , nx-2, ny, nx, ny+2, colr, colg, colb, s);
					}
					else
					{
						draw_line(vid , nx-2, ny+2, nx+2, ny+2, colr, colg, colb, s);
						draw_line(vid , nx-2, ny-2, nx+2, ny-2, colr, colg, colb, s);
						draw_line(vid , nx-2, ny-2, nx-2, ny+2, colr, colg, colb, s);
						draw_line(vid , nx+2, ny-2, nx+2, ny+2, colr, colg, colb, s);
					}
					//legs
					draw_line(vid , nx, ny+3, (int)cplayer->legs[0], (int)cplayer->legs[1], legr, legg, legb, s);
					draw_line(vid , (int)cplayer->legs[0], (int)cplayer->legs[1], (int)cplayer->legs[4], (int)cplayer->legs[5], legr, legg, legb, s);
					draw_line(vid , nx, ny+3, (int)cplayer->legs[8], (int)cplayer->legs[9], legr, legg, legb, s);
					draw_line(vid , (int)cplayer->legs[8], (int)cplayer->legs[9], (int)cplayer->legs[12], (int)cplayer->legs[13], legr, legg, legb, s);
					if (cplayer->rocketBoots)
					{
						int leg;
						for (leg=0; leg<2; leg++)
						{
							int nx = (int)cplayer->legs[leg*8+4], ny = (int)cplayer->legs[leg*8+5];
							int colr = 255, colg = 0, colb = 255;
							if (((int)(cplayer->comm)&0x04) == 0x04 || (((int)(cplayer->comm)&0x01) == 0x01 && leg==0) || (((int)(cplayer->comm)&0x02) == 0x02 && leg==1))
								blendpixel(vid, nx, ny, 0, 255, 0, 255);
							else
								blendpixel(vid, nx, ny, 255, 0, 0, 255);
							blendpixel(vid, nx+1, ny, colr, colg, colb, 223);
							blendpixel(vid, nx-1, ny, colr, colg, colb, 223);
							blendpixel(vid, nx, ny+1, colr, colg, colb, 223);
							blendpixel(vid, nx, ny-1, colr, colg, colb, 223);

							blendpixel(vid, nx+1, ny-1, colr, colg, colb, 112);
							blendpixel(vid, nx-1, ny-1, colr, colg, colb, 112);
							blendpixel(vid, nx+1, ny+1, colr, colg, colb, 112);
							blendpixel(vid, nx-1, ny+1, colr, colg, colb, 112);
						}
					}
				}
				if(pixel_mode & PMODE_FLAT)
				{
					drawpixel(vid, nx, ny, colr, colg, colb, 255);
				}
				if(pixel_mode & PMODE_BLEND)
				{
					blendpixel(vid, nx, ny, colr, colg, colb, cola);
				}
				if(pixel_mode & PMODE_ADD)
				{
					addpixel(vid, nx, ny, colr, colg, colb, cola);
				}
				if(pixel_mode & PMODE_BLOB)
				{
					vid[ny*(XRES+BARSIZE)+nx] = PIXRGB(colr,colg,colb);

					blendpixel(vid, nx+1, ny, colr, colg, colb, 223);
					blendpixel(vid, nx-1, ny, colr, colg, colb, 223);
					blendpixel(vid, nx, ny+1, colr, colg, colb, 223);
					blendpixel(vid, nx, ny-1, colr, colg, colb, 223);

					blendpixel(vid, nx+1, ny-1, colr, colg, colb, 112);
					blendpixel(vid, nx-1, ny-1, colr, colg, colb, 112);
					blendpixel(vid, nx+1, ny+1, colr, colg, colb, 112);
					blendpixel(vid, nx-1, ny+1, colr, colg, colb, 112);
				}
				if(pixel_mode & PMODE_GLOW)
				{
					int cola1 = (5*cola)/255;
					addpixel(vid, nx, ny, colr, colg, colb, (192*cola)/255);
					addpixel(vid, nx+1, ny, colr, colg, colb, (96*cola)/255);
					addpixel(vid, nx-1, ny, colr, colg, colb, (96*cola)/255);
					addpixel(vid, nx, ny+1, colr, colg, colb, (96*cola)/255);
					addpixel(vid, nx, ny-1, colr, colg, colb, (96*cola)/255);
					
					for (x = 1; x < 6; x++) {
						addpixel(vid, nx, ny-x, colr, colg, colb, cola1);
						addpixel(vid, nx, ny+x, colr, colg, colb, cola1);
						addpixel(vid, nx-x, ny, colr, colg, colb, cola1);
						addpixel(vid, nx+x, ny, colr, colg, colb, cola1);
						for (y = 1; y < 6; y++) {
							if(x + y > 7)
								continue;
							addpixel(vid, nx+x, ny-y, colr, colg, colb, cola1);
							addpixel(vid, nx-x, ny+y, colr, colg, colb, cola1);
							addpixel(vid, nx+x, ny+y, colr, colg, colb, cola1);
							addpixel(vid, nx-x, ny-y, colr, colg, colb, cola1);
						}
					}
				}
				if(pixel_mode & PMODE_BLUR)
				{
					for (x=-3; x<4; x++)
					{
						for (y=-3; y<4; y++)
						{
							if (abs(x)+abs(y) <2 && !(abs(x)==2||abs(y)==2))
								blendpixel(vid, x+nx, y+ny, colr, colg, colb, 30);
							if (abs(x)+abs(y) <=3 && abs(x)+abs(y))
								blendpixel(vid, x+nx, y+ny, colr, colg, colb, 20);
							if (abs(x)+abs(y) == 2)
								blendpixel(vid, x+nx, y+ny, colr, colg, colb, 10);
						}
					}
				}
				if(pixel_mode & PMODE_SPARK)
				{
					flicker = (float)(rand()%20);
					gradv = 4*parts[i].life + flicker;
					for (x = 0; (gradv>0.5) && (drawing_budget > 0); x++) {
						addpixel(vid, nx+x, ny, colr, colg, colb, (int)gradv);
						addpixel(vid, nx-x, ny, colr, colg, colb, (int)gradv);

						addpixel(vid, nx, ny+x, colr, colg, colb, (int)gradv);
						addpixel(vid, nx, ny-x, colr, colg, colb, (int)gradv);
						gradv = gradv/1.5f;
						drawing_budget--;
					}
				}
				if(pixel_mode & PMODE_FLARE)
				{
					flicker = (float)(rand()%20);
					gradv = flicker + fabs(parts[i].vx)*17 + fabs(parts[i].vy)*17;
					blendpixel(vid, nx, ny, colr, colg, colb, (int)((gradv*4)>255?255:(gradv*4)));
					blendpixel(vid, nx+1, ny, colr, colg, colb, (int)((gradv*2)>255?255:(gradv*2)));
					blendpixel(vid, nx-1, ny, colr, colg, colb, (int)((gradv*2)>255?255:(gradv*2)));
					blendpixel(vid, nx, ny+1, colr, colg, colb, (int)((gradv*2)>255?255:(gradv*2)));
					blendpixel(vid, nx, ny-1, colr, colg, colb, (int)((gradv*2)>255?255:(gradv*2)));
					if (gradv>255) gradv=255;
					blendpixel(vid, nx+1, ny-1, colr, colg, colb, (int)gradv);
					blendpixel(vid, nx-1, ny-1, colr, colg, colb, (int)gradv);
					blendpixel(vid, nx+1, ny+1, colr, colg, colb, (int)gradv);
					blendpixel(vid, nx-1, ny+1, colr, colg, colb, (int)gradv);
					for (x = 1; (gradv>0.5) && (drawing_budget > 0); x++) {
						addpixel(vid, nx+x, ny, colr, colg, colb, (int)gradv);
						addpixel(vid, nx-x, ny, colr, colg, colb, (int)gradv);
						addpixel(vid, nx, ny+x, colr, colg, colb, (int)gradv);
						addpixel(vid, nx, ny-x, colr, colg, colb, (int)gradv);
						gradv = gradv/1.2f;
						drawing_budget--;
					}
				}
				if(pixel_mode & PMODE_LFLARE)
				{
					flicker = (float)(rand()%20);
					gradv = flicker + fabs(parts[i].vx)*17 + fabs(parts[i].vy)*17;
					blendpixel(vid, nx, ny, colr, colg, colb, (int)((gradv*4)>255?255:(gradv*4)));
					blendpixel(vid, nx+1, ny, colr, colg, colb, (int)((gradv*2)>255?255:(gradv*2)));
					blendpixel(vid, nx-1, ny, colr, colg, colb, (int)((gradv*2)>255?255:(gradv*2)));
					blendpixel(vid, nx, ny+1, colr, colg, colb, (int)((gradv*2)>255?255:(gradv*2)));
					blendpixel(vid, nx, ny-1, colr, colg, colb, (int)((gradv*2)>255?255:(gradv*2)));
					if (gradv>255) gradv=255;
					blendpixel(vid, nx+1, ny-1, colr, colg, colb, (int)gradv);
					blendpixel(vid, nx-1, ny-1, colr, colg, colb, (int)gradv);
					blendpixel(vid, nx+1, ny+1, colr, colg, colb, (int)gradv);
					blendpixel(vid, nx-1, ny+1, colr, colg, colb, (int)gradv);
					for (x = 1; (gradv>0.5) && (drawing_budget > 0); x++) {
						addpixel(vid, nx+x, ny, colr, colg, colb, (int)gradv);
						addpixel(vid, nx-x, ny, colr, colg, colb, (int)gradv);
						addpixel(vid, nx, ny+x, colr, colg, colb, (int)gradv);
						addpixel(vid, nx, ny-x, colr, colg, colb, (int)gradv);
						gradv = gradv/1.01f;
						drawing_budget--;
					}
				}
				if (pixel_mode & EFFECT_GRAVIN)
				{
					int nxo = 0;
					int nyo = 0;
					int r;
					float drad = 0.0f;
					float ddist = 0.0f;
					orbitalparts_get(parts[i].life, parts[i].ctype, orbd, orbl);
					for (r = 0; r < 4; r++) {
						ddist = ((float)orbd[r])/16.0f;
						drad = (M_PI * ((float)orbl[r]) / 180.0f)*1.41f;
						nxo = (int)(ddist*cos(drad));
						nyo = (int)(ddist*sin(drad));
#ifdef NOMOD
						if (ny+nyo>0 && ny+nyo<YRES && nx+nxo>0 && nx+nxo<XRES && TYP(pmap[ny+nyo][nx+nxo]) != PT_PRTI)
							addpixel(vid, nx+nxo, ny+nyo, colr, colg, colb, 255-orbd[r]);
#else
						if (ny+nyo>0 && ny+nyo<YRES && nx+nxo>0 && nx+nxo<XRES && TYP(pmap[ny+nyo][nx+nxo]) != PT_PRTI && TYP(pmap[ny+nyo][nx+nxo]) != PT_PPTI)
							addpixel(vid, nx+nxo, ny+nyo, colr, colg, colb, 255-orbd[r]);
#endif
					}
				}
				if (pixel_mode & EFFECT_GRAVOUT)
				{
					int nxo = 0;
					int nyo = 0;
					int r;
					float drad = 0.0f;
					float ddist = 0.0f;
					orbitalparts_get(parts[i].life, parts[i].ctype, orbd, orbl);
					for (r = 0; r < 4; r++) {
						ddist = ((float)orbd[r])/16.0f;
						drad = (M_PI * ((float)orbl[r]) / 180.0f)*1.41f;
						nxo = (int)(ddist*cos(drad));
						nyo = (int)(ddist*sin(drad));
#ifdef NOMOD
						if (ny+nyo>0 && ny+nyo<YRES && nx+nxo>0 && nx+nxo<XRES && TYP(pmap[ny+nyo][nx+nxo]) != PT_PRTO)
							addpixel(vid, nx+nxo, ny+nyo, colr, colg, colb, 255-orbd[r]);
#else
						if (ny+nyo>0 && ny+nyo<YRES && nx+nxo>0 && nx+nxo<XRES && TYP(pmap[ny+nyo][nx+nxo]) != PT_PRTO && TYP(pmap[ny+nyo][nx+nxo]) != PT_PPTO)
							addpixel(vid, nx+nxo, ny+nyo, colr, colg, colb, 255-orbd[r]);
#endif
					}
				}
				if ((pixel_mode & EFFECT_DBGLINES) && DEBUG_MODE && !(display_mode&DISPLAY_PERS))
				{
					// draw lines connecting wifi/portal channels
					if (mousePos.X == nx && mousePos.Y == ny && ((unsigned int)i == ID(pmap[ny][nx])))
					{
						int type = parts[i].type, tmp = (int)((parts[i].temp-73.15f)/100+1), othertmp;
						int type2 = parts[i].type;
#ifndef NOMOD
						if (type == PT_PRTI || type == PT_PPTI)
							type = PT_PRTO;
						else if (type == PT_PRTO || type == PT_PPTO)
							type = PT_PRTI;
#else
						if (type == PT_PRTI)
							type = PT_PRTO;
						else if (type == PT_PRTO )
							type = PT_PRTI;
#endif
#ifndef NOMOD
						if (type == PT_PRTI)
							type2 = PT_PPTI;
						else if (type == PT_PRTO)
							type2 = PT_PPTO;
#endif
						for (int z = 0; z <= sim->parts_lastActiveIndex; z++)
						{
							if (parts[z].type==type || parts[z].type==type2)
							{
								othertmp = (int)((parts[z].temp-73.15f)/100+1); 
								if (tmp == othertmp)
									xor_line(nx,ny,(int)(parts[z].x+0.5f),(int)(parts[z].y+0.5f),vid);
							}
						}
					}
				}
				//Fire effects
				if(firea && (pixel_mode & FIRE_BLEND))
				{
					firea /= 2;
					fire_r[ny/CELL][nx/CELL] = (firea*firer + (255-firea)*fire_r[ny/CELL][nx/CELL]) >> 8;
					fire_g[ny/CELL][nx/CELL] = (firea*fireg + (255-firea)*fire_g[ny/CELL][nx/CELL]) >> 8;
					fire_b[ny/CELL][nx/CELL] = (firea*fireb + (255-firea)*fire_b[ny/CELL][nx/CELL]) >> 8;
				}
				if(firea && (pixel_mode & FIRE_ADD))
				{
					firea /= 8;
					firer = ((firea*firer) >> 8) + fire_r[ny/CELL][nx/CELL];
					fireg = ((firea*fireg) >> 8) + fire_g[ny/CELL][nx/CELL];
					fireb = ((firea*fireb) >> 8) + fire_b[ny/CELL][nx/CELL];
				
					if(firer>255)
						firer = 255;
					if(fireg>255)
						fireg = 255;
					if(fireb>255)
						fireb = 255;
					
					fire_r[ny/CELL][nx/CELL] = firer;
					fire_g[ny/CELL][nx/CELL] = fireg;
					fire_b[ny/CELL][nx/CELL] = fireb;
				}
				if(firea && (pixel_mode & FIRE_SPARK))
				{
					firea /= 4;
					fire_r[ny/CELL][nx/CELL] = (firea*firer + (255-firea)*fire_r[ny/CELL][nx/CELL]) >> 8;
					fire_g[ny/CELL][nx/CELL] = (firea*fireg + (255-firea)*fire_g[ny/CELL][nx/CELL]) >> 8;
					fire_b[ny/CELL][nx/CELL] = (firea*fireb + (255-firea)*fire_b[ny/CELL][nx/CELL]) >> 8;
				}
			}
		}
	}
}

// draw the graphics that appear before update_particles is called
void render_before(pixel *part_vbuf, Simulation * sim)
{
		if (Renderer::Ref().GetDisplayMode() & DISPLAY_AIR)//air only gets drawn in these modes
		{
			draw_air(part_vbuf, sim);
		}
		else if (Renderer::Ref().GetDisplayMode() & DISPLAY_PERS)//save background for persistent, then clear
		{
			memcpy(part_vbuf, pers_bg, (XRES+BARSIZE)*YRES*PIXELSIZE);
			memset(part_vbuf+((XRES+BARSIZE)*YRES), 0, ((XRES+BARSIZE)*YRES*PIXELSIZE)-((XRES+BARSIZE)*YRES*PIXELSIZE));
		}
		else //clear screen every frame
		{
			memset(part_vbuf, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
		}
#ifdef LUACONSOLE
		auto ev = BeforeSimDrawEvent();
		HandleEvent(LuaEvents::beforesimdraw, &ev);
#endif
		if (sim->grav->IsEnabled() && drawgrav_enable)
			draw_grav(part_vbuf);
		draw_walls(part_vbuf, sim);
}

int persist_counter = 0;
// draw the graphics that appear after update_particles is called
void render_after(pixel *part_vbuf, pixel *vid_buf, Simulation * sim, Point mousePos)
{
	render_parts(part_vbuf, sim, mousePos); //draw particles
	if (vid_buf && (Renderer::Ref().GetDisplayMode() & DISPLAY_PERS))
	{
		if (!persist_counter)
		{
			dim_copy_pers(pers_bg, vid_buf);
		}
		else
		{
			memcpy(pers_bg, vid_buf, (XRES+BARSIZE)*YRES*PIXELSIZE);
		}
		persist_counter = (persist_counter+1) % 3;
	}
#ifndef OGLR
	if (Renderer::Ref().GetRenderMode() & FIREMODE)
		render_fire(part_vbuf);
#endif
	draw_other(part_vbuf, sim);
#ifndef RENDERER
	if (((WallTool*)activeTools[the_game->GetToolIndex()])->GetID() == WL_GRAV)
		draw_grav_zones(part_vbuf);
#endif

	render_signs(part_vbuf, sim);

#ifndef OGLR
	if (vid_buf && sim->grav->IsEnabled() && (Renderer::Ref().GetDisplayMode() & DISPLAY_WARP))
		render_gravlensing(part_vbuf, vid_buf);
#endif

	if (finding & 0x8)
		draw_find(sim);
	sampleColor = vid_buf[(mousePos.Y)*(XRES + BARSIZE) + (mousePos.X)];
}

// Find just like how my lua script did it, it will find everything and show it's exact spot, and not miss things under stacked particles
void draw_find(Simulation * sim)
{
	if (finding == 0x8)
		return;
	// Dim everything
	fillrect(vid_buf, -1, -1, XRES+1, YRES+1, 0, 0, 0, 230);
	foundParticles = 0;
	// Color particles
	for (int i = 0; i <= sim->parts_lastActiveIndex; i++)
	{
		if (!parts[i].type)
			continue;
		if ((finding & 0x1) && find_matches(i, activeTools[0]))
		{
			drawpixel(vid_buf, (int)(parts[i].x+.5f), (int)(parts[i].y+.5f), 255, 0, 0, 255);
			foundParticles++;
		}
		else if ((finding & 0x2) && find_matches(i, activeTools[1]))
		{
			drawpixel(vid_buf, (int)(parts[i].x+.5f), (int)(parts[i].y+.5f), 0, 0, 255, 255);
			foundParticles++;
		}
		else if ((finding & 0x4) && find_matches(i, activeTools[2]))
		{
			drawpixel(vid_buf, (int)(parts[i].x+.5f), (int)(parts[i].y+.5f), 0, 255, 0, 255);
			foundParticles++;
		}
	}
	// Color walls
	for (int y = 0; y < YRES/CELL; y++)
	{
		for (int x = 0; x < XRES/CELL; x++)
		{
			if ((finding & 0x1) && bmap[y][x] == ((WallTool*)activeTools[0])->GetID())
				fillrect(vid_buf, x*CELL-1, y*CELL-1, CELL+1, CELL+1, 255, 0, 0, 255);
			else if ((finding & 0x2) && bmap[y][x] == ((WallTool*)activeTools[1])->GetID())
				fillrect(vid_buf, x*CELL-1, y*CELL-1, CELL+1, CELL+1, 0, 0, 255, 255);
			else if ((finding & 0x4) && bmap[y][x] == ((WallTool*)activeTools[2])->GetID())
				fillrect(vid_buf, x*CELL-1, y*CELL-1, CELL+1, CELL+1, 0, 255, 0, 255);
		}
	}
}

/**
 * Check if find tool matches particle i, based on the active tool. Has special cases for PROP tool to match selected property
 */
bool find_matches(int i, Tool *activeTool)
{
	if ((parts[i].type != PT_LIFE && ((ElementTool*)activeTool)->GetID() == parts[i].type) || (parts[i].type == PT_LIFE && ((GolTool*)activeTool)->GetID() == parts[i].ctype))
	{
		return true;
	}

	if (((ToolTool*)activeTool)->GetID() == TOOL_PROP && !((PropTool*)activeTool)->invalidState)
	{
		StructProperty propType = ((PropTool*)activeTool)->prop;
		PropertyValue propValue = ((PropTool*)activeTool)->propValue;

		bool matches = false;
		switch (propType.Type)
		{
		case StructProperty::Float:
			matches = *((float*)(((char*)&parts[i]) + propType.Offset)) == propValue.Float;
			break;

		case StructProperty::ParticleType:
		case StructProperty::Integer:
			matches = *((int*)(((char*)&parts[i]) + propType.Offset)) == propValue.Integer;
			break;

		case StructProperty::UInteger:
			matches = *((unsigned int*)(((char*)&parts[i]) + propType.Offset)) == propValue.UInteger;
			break;

		default:
			break;
		}

		return matches;
	}

	return false;
}

void draw_walls(pixel *vid, Simulation * sim)
{
	for (int y = 0; y < YRES/CELL; y++)
		for (int x =0; x < XRES/CELL; x++)
			if (bmap[y][x])
			{
				unsigned char wt = bmap[y][x];
				if (wt >= WALLCOUNT)
					continue;
				unsigned char powered = emap[y][x];
				pixel pc = PIXPACK(wallTypes[wt].colour);
				pixel gc = PIXPACK(wallTypes[wt].eglow);

				if (finding)
				{
					if ((finding & 0x1) && wt == ((WallTool*)activeTools[0])->GetID())
					{
						pc = PIXRGB(255,0,0);
						gc = PIXRGB(255,0,0);
					}
					else if ((finding & 0x2) && wt == ((WallTool*)activeTools[1])->GetID())
					{
						pc = PIXRGB(0,0,255);
						gc = PIXRGB(0,0,255);
					}
					else if ((finding & 0x4) && wt == ((WallTool*)activeTools[2])->GetID())
					{
						pc = PIXRGB(0,255,0);
						gc = PIXRGB(0,255,0);
					}
					else if (!(finding &0x8))
					{
						pc = PIXRGB(PIXR(pc)/10,PIXG(pc)/10,PIXB(pc)/10);
						gc = PIXRGB(PIXR(gc)/10,PIXG(gc)/10,PIXB(gc)/10);
					}
				}

				switch (wallTypes[wt].drawstyle)
				{
				case 0:
					if (wt == WL_EWALL || wt == WL_STASIS)
					{
						bool reverse = wt == WL_STASIS;
						if ((powered > 0) ^ reverse)
						{
							for (int j = 0; j < CELL; j++)
								for (int i =0; i < CELL; i++)
									if (i&j&1)
										vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = pc;
						}
						else
						{
							for (int j = 0; j < CELL; j++)
								for (int i = 0; i < CELL; i++)
									if (!(i&j&1))
										vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = pc;
						}
					}
					else if (wt == WL_WALLELEC)
					{
						for (int j = 0; j < CELL; j++)
							for (int i = 0; i < CELL; i++)
							{
								if (!((y*CELL+j)%2) && !((x*CELL+i)%2))
									vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = pc;
								else
									vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = PIXPACK(0x808080);
							}
					}
					else if (wt == WL_EHOLE)
					{
						if (powered)
						{
							for (int j = 0; j < CELL; j++)
								for (int i = 0; i < CELL; i++)
									vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = PIXPACK(0x242424);
							for (int j = 0; j < CELL; j += 2)
								for (int i = 0; i < CELL; i += 2)
									vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = PIXPACK(0x000000);
						}
						else
						{
							for (int j = 0; j < CELL; j += 2)
								for (int i =0; i < CELL; i += 2)
									vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = PIXPACK(0x242424);
						}
					}
					else if (wt == WL_STREAM)
					{
						float xf = x*CELL + CELL*0.5f;
						float yf = y*CELL + CELL*0.5f;
						int oldX = (int)(xf+0.5f), oldY = (int)(yf+0.5f);
						int newX, newY;
						float xVel = sim->air->vx[y][x]*0.125f, yVel = sim->air->vy[y][x]*0.125f;
						// there is no velocity here, draw a streamline and continue
						if (!xVel && !yVel)
						{
							drawtext(vid, x*CELL, y*CELL-2, "\x8D", 255, 255, 255, 128);
							drawpixel(vid, oldX, oldY, 255, 255, 255, 255);
							continue;
						}
						bool changed = false;
						for (int t = 0; t < 1024; t++)
						{
							newX = (int)(xf+0.5f);
							newY = (int)(yf+0.5f);
							if (newX != oldX || newY != oldY)
							{
								changed = true;
								oldX = newX;
								oldY = newY;
							}
							if (changed && (newX<0 || newX>=XRES || newY<0 || newY>=YRES))
								break;
							addpixel(vid, newX, newY, 255, 255, 255, 64);
							// cache velocity and other checks so we aren't running them constantly
							if (changed)
							{
								int wallX = newX/CELL;
								int wallY = newY/CELL;
								xVel = sim->air->vx[wallY][wallX]*0.125f;
								yVel = sim->air->vy[wallY][wallX]*0.125f;
								if (wallX != x && wallY != y && bmap[wallY][wallX] == WL_STREAM)
									break;
							}
							xf += xVel;
							yf += yVel;
						}
						drawtext(vid, x*CELL, y*CELL-2, "\x8D", 255, 255, 255, 128);
					}
					break;
				case 1:
					for (int j = 0; j < CELL; j += 2)
						for (int i = (j>>1)&1; i < CELL; i += 2)
							vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = pc;
					break;
				case 2:
					for (int j = 0; j < CELL; j += 2)
						for (int i = 0; i < CELL; i += 2)
							vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = pc;
					break;
				case 3:
					for (int j = 0; j < CELL; j++)
						for (int i = 0; i < CELL; i++)
							vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = pc;
					break;
				case 4:
					for (int j = 0; j < CELL; j++)
						for (int i = 0; i < CELL; i++)
							if (i == j)
								vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = pc;
							else if (i == j+1 || (i == 0 && j == CELL-1))
								vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = gc;
							else
								vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = PIXPACK(0x202020);
					break;
				}

				// when in blob view, draw some blobs...
				if (Renderer::Ref().GetRenderMode() & PMODE_BLOB)
				{
					switch (wallTypes[wt].drawstyle)
					{
					case 0:
						if (wt == WL_EWALL || wt == WL_STASIS)
						{
							bool reverse = wt == WL_STASIS;
							if ((powered > 0) ^ reverse)
							{
								for (int j = 0; j < CELL; j++)
									for (int i =0; i < CELL; i++)
										if (i&j&1)
											drawblob(vid, (x*CELL+i), (y*CELL+j), PIXR(pc), PIXG(pc), PIXB(pc));
							}
							else
							{
								for (int j = 0; j < CELL; j++)
									for (int i = 0; i < CELL; i++)
										if (!(i&j&1))
											drawblob(vid, (x*CELL+i), (y*CELL+j), PIXR(pc), PIXG(pc), PIXB(pc));
							}
						}
						else if (wt == WL_WALLELEC)
						{
							for (int j = 0; j < CELL; j++)
								for (int i =0; i < CELL; i++)
								{
									if (!((y*CELL+j)%2) && !((x*CELL+i)%2))
										drawblob(vid, (x*CELL+i), (y*CELL+j), PIXR(pc), PIXG(pc), PIXB(pc));
									else
										drawblob(vid, (x*CELL+i), (y*CELL+j), 0x80, 0x80, 0x80);
								}
						}
						else if (wt == WL_EHOLE)
						{
							if (powered)
							{
								for (int j = 0; j < CELL; j++)
									for (int i = 0; i < CELL; i++)
										drawblob(vid, (x*CELL+i), (y*CELL+j), 0x24, 0x24, 0x24);
								for (int j = 0; j < CELL; j += 2)
									for (int i = 0; i < CELL; i += 2)
										// looks bad if drawing black blobs
										vid[(y*CELL+j)*(XRES+BARSIZE)+(x*CELL+i)] = PIXPACK(0x000000);
							}
							else
							{
								for (int j = 0; j < CELL; j += 2)
									for (int i = 0; i < CELL; i += 2)
										drawblob(vid, (x*CELL+i), (y*CELL+j), 0x24, 0x24, 0x24);
							}
						}
						break;
					 case 1:
						for (int j = 0; j < CELL; j += 2)
							for (int i = (j>>1)&1; i < CELL; i += 2)
								drawblob(vid, (x*CELL+i), (y*CELL+j), PIXR(pc), PIXG(pc), PIXB(pc));
						break;
					case 2:
						for (int j = 0; j < CELL; j += 2)
							for (int i = 0; i < CELL; i+=2)
								drawblob(vid, (x*CELL+i), (y*CELL+j), PIXR(pc), PIXG(pc), PIXB(pc));
						break;
					case 3:
						for (int j = 0; j < CELL; j++)
							for (int i = 0; i < CELL; i++)
								drawblob(vid, (x*CELL+i), (y*CELL+j), PIXR(pc), PIXG(pc), PIXB(pc));
						break;
					case 4:
						for (int j = 0; j < CELL; j++)
							for (int i = 0; i < CELL; i++)
								if (i == j)
									drawblob(vid, (x*CELL+i), (y*CELL+j), PIXR(pc), PIXG(pc), PIXB(pc));
								else if (i == j+1 || (i == 0 && j == CELL-1))
									drawblob(vid, (x*CELL+i), (y*CELL+j), PIXR(gc), PIXG(gc), PIXB(gc));
								else
									drawblob(vid, (x*CELL+i), (y*CELL+j), 0x20, 0x20, 0x20);
						break;
					}
				}

				if (wallTypes[wt].eglow && powered)
				{
					// glow if electrified
					ARGBColour glow = wallTypes[wt].eglow;
					int alpha = 255;
					int cr = (alpha*COLR(glow) + (255-alpha)*fire_r[y/CELL][x/CELL]) >> 8;
					int cg = (alpha*COLG(glow) + (255-alpha)*fire_g[y/CELL][x/CELL]) >> 8;
					int cb = (alpha*COLB(glow) + (255-alpha)*fire_b[y/CELL][x/CELL]) >> 8;

					if (cr > 255)
						cr = 255;
					if (cg > 255)
						cg = 255;
					if (cb > 255)
						cb = 255;
					fire_r[y][x] = cr;
					fire_g[y][x] = cg;
					fire_b[y][x] = cb;
				}
			}
}

void render_signs(pixel *vid_buf, Simulation * sim)
{
	int x, y, w, h;
	Sign::Justification ju;
	for (const Sign& sign : signs)
	{
		sign.GetPos(sim, x, y, w, h);
		clearrect(vid_buf, x+1, y+1, w-1, h-1);
		drawrect(vid_buf, x, y, w, h, 192, 192, 192, 255);

		// spark signs and link signs have different colors
		ARGBColour textCol;
		switch (sign.GetType())
		{
		default:
		case Sign::Normal:
			textCol = COLPACK(0xFFFFFF);
			break;
		case Sign::SaveLink:
			textCol = COLPACK(0x00BFFF);
			break;
		case Sign::ThreadLink:
			textCol = COLPACK(0xFF4B4B);
			break;
		case Sign::Spark:
			textCol = COLPACK(0xD3D328);
			break;
		case Sign::SearchLink:
			textCol = COLPACK(0x9353D3);
			break;
		}
		drawtext(vid_buf, x+3, y+4, sign.GetDisplayText(sim).c_str(), COLR(textCol), COLG(textCol), COLB(textCol), COLA(textCol));

		// draw the little line on the buttom
		Point realPos = sign.GetRealPos();
		ju = sign.GetJustification();
		if (ju != Sign::NoJustification)
		{
			int dx = 1 - (int)ju;
			int dy = (realPos.Y > 18) ? -1 : 1;
			for (int i = 0; i < 4; i++)
			{
				drawpixel(vid_buf, realPos.X, realPos.Y, 192, 192, 192, 255);
				realPos += Point(dx, dy);
			}
		}
	}
}

void render_gravlensing(pixel *src, pixel * dst)
{
	int nx, ny, rx, ry, gx, gy, bx, by, co;
	int r, g, b;
	pixel t;
	for(nx = 0; nx < XRES; nx++)
	{
		for(ny = 0; ny < YRES; ny++)
		{
			co = (ny/CELL)*(XRES/CELL)+(nx/CELL);
			rx = (int)(nx - globalSim->grav->gravx[co] * 0.75f + 0.5f);
			ry = (int)(ny - globalSim->grav->gravy[co] * 0.75f + 0.5f);
			gx = (int)(nx - globalSim->grav->gravx[co] * 0.875f + 0.5f);
			gy = (int)(ny - globalSim->grav->gravy[co] * 0.875f + 0.5f);
			bx = (int)(nx - globalSim->grav->gravx[co] + 0.5f);
			by = (int)(ny - globalSim->grav->gravy[co] + 0.5f);
			if(rx >= 0 && rx < XRES && ry >= 0 && ry < YRES && gx >= 0 && gx < XRES && gy >= 0 && gy < YRES && bx >= 0 && bx < XRES && by >= 0 && by < YRES)
			{
				t = dst[ny*(XRES+BARSIZE)+nx];
				r = PIXR(src[ry*(XRES+BARSIZE)+rx]) + PIXR(t);
				g = PIXG(src[gy*(XRES+BARSIZE)+gx]) + PIXG(t);
				b = PIXB(src[by*(XRES+BARSIZE)+bx]) + PIXB(t);
				if (r>255)
					r = 255;
				if (g>255)
					g = 255;
				if (b>255)
					b = 255;
				dst[ny*(XRES+BARSIZE)+nx] = PIXRGB(r,g,b);
			}
		}
	}
}

void render_fire(pixel *vid)
{
	int i,j,x,y,r,g,b,a;
	for (j=0; j<YRES/CELL; j++)
		for (i=0; i<XRES/CELL; i++)
		{
			r = fire_r[j][i];
			g = fire_g[j][i];
			b = fire_b[j][i];
			if (r || g || b)
				for (y=-CELL; y<2*CELL; y++)
					for (x=-CELL; x<2*CELL; x++)
					{
						a = fire_alpha[y+CELL][x+CELL];
						if (finding && !(finding & 0x8))
							a /= 2;
						addpixel(vid, i*CELL+x, j*CELL+y, r, g, b, a);
					}
			r *= 8;
			g *= 8;
			b *= 8;
			for (y=-1; y<2; y++)
				for (x=-1; x<2; x++)
					if ((x || y) && i+x>=0 && j+y>=0 && i+x<XRES/CELL && j+y<YRES/CELL)
					{
						r += fire_r[j+y][i+x];
						g += fire_g[j+y][i+x];
						b += fire_b[j+y][i+x];
					}
			r /= 16;
			g /= 16;
			b /= 16;
			fire_r[j][i] = r>4 ? r-4 : 0;
			fire_g[j][i] = g>4 ? g-4 : 0;
			fire_b[j][i] = b>4 ? b-4 : 0;
		}
}

void prepare_alpha(float intensity)
{
	fireIntensity = intensity;
	float multiplier = 255.0f * fireIntensity;
	float temp[CELL*3][CELL*3];
	memset(temp, 0, sizeof(temp));
	for (int x = 0; x < CELL; x++)
		for (int y = 0; y < CELL; y++)
			for (int i = -CELL; i < CELL; i++)
				for (int j = -CELL; j < CELL; j++)
					temp[y+CELL+j][x+CELL+i] += expf(-0.1f*(i*i+j*j));
	for (int x = 0; x < CELL * 3; x++)
		for (int y = 0; y < CELL * 3; y++)
			fire_alpha[y][x] = (int)(multiplier*temp[y][x]/(CELL*CELL));
}

pixel *render_packed_rgb(void *image, int width, int height, int cmp_size)
{
	unsigned char *tmp;
	pixel *res;
	int i;

	tmp = (unsigned char*)malloc(width*height*3);
	if (!tmp)
		return NULL;
	res = (pixel*)malloc(width*height*PIXELSIZE);
	if (!res)
	{
		free(tmp);
		return NULL;
	}

	i = width*height*3;
	if (BZ2_bzBuffToBuffDecompress((char *)tmp, (unsigned *)&i, (char *)image, cmp_size, 0, 0))
	{
		free(res);
		free(tmp);
		return NULL;
	}

	for (i=0; i<width*height; i++)
		res[i] = PIXRGB(tmp[3*i], tmp[3*i+1], tmp[3*i+2]);

	free(tmp);
	return res;
}

void draw_rgba_image(pixel *vid, unsigned char *data, int x, int y, float alpha)
{
	unsigned char w, h;
	int i, j;
	unsigned char r, g, b, a;
	if (!data) return;
	w = *(data++)&0xFF;
	h = *(data++)&0xFF;
	for (j=0; j<h; j++)
	{
		for (i=0; i<w; i++)
		{
			r = *(data++)&0xFF;
			g = *(data++)&0xFF;
			b = *(data++)&0xFF;
			a = *(data++)&0xFF;
			drawpixel(vid, x+i, y+j, r, g, b, (int)(a*alpha));
		}
	}
}

void draw_image(pixel *vid, pixel *img, int x, int y, int w, int h, int a)
{
	int startX = 0, skipEnd = 0;
	if (!img)
		return;
	// Adjust height to prevent drawing off the bottom
	if (y + h > VIDYRES)
		h = ((VIDYRES) - y) - 1;
	// Adjust width to prevent drawing off the right
	if (x + w > VIDXRES)
		skipEnd = (x + w) - VIDXRES;

	// Starts off the top of the screen, adjust
	if (y < 0 && -y < h)
	{
		img += -y*w;
		h += y;
		y = 0;
	}
	// Starts off the left side of the screen, adjust
	if (x < 0 && -x < w)
	{
		startX = -x;
	}

	if (!h || y < 0 || !w)
		return;
	if (a >= 255)
		for (int j = 0; j < h; j++)
		{
			img += startX;
			for (int i = startX; i < w && x + i < VIDXRES; i++)
			{
				vid[(y+j)*(VIDXRES)+(x+i)] = *img;
				img++;
			}
			img += skipEnd;
		}
	else
	{
		int r, g, b;
		for (int j = 0; j < h; j++)
		{
			img += startX;
			for (int i = startX; i < w && x + i < VIDXRES; i++)
			{
				r = PIXR(*img);
				g = PIXG(*img);
				b = PIXB(*img);
				blendpixel(vid, x+i, y+j, r, g, b, a);
				img++;
			}
			img += skipEnd;
		}
	}
}

void dim_copy(pixel *dst, pixel *src) //old persistent, unused
{
	int i,r,g,b;
	for (i=0; i<XRES*YRES; i++)
	{
		r = PIXR(src[i]);
		g = PIXG(src[i]);
		b = PIXB(src[i]);
		if (r>0)
			r--;
		if (g>0)
			g--;
		if (b>0)
			b--;
		dst[i] = PIXRGB(r,g,b);
	}
}

void dim_copy_pers(pixel *dst, pixel *src) //for persistent view, reduces rgb slowly
{
	int i,r,g,b;
	for (i=0; i<(XRES+BARSIZE)*YRES; i++)
	{
		r = PIXR(src[i]);
		g = PIXG(src[i]);
		b = PIXB(src[i]);
		if (r>0)
			r--;
		if (g>0)
			g--;
		if (b>0)
			b--;
		dst[i] = PIXRGB(r,g,b);
	}
}

void render_zoom(pixel *img)
{
	Point zoomScopePosition = the_game->GetZoomScopePosition();
	Point zoomWindowPosition = the_game->GetZoomWindowPosition();
	int zoomScopeSize = the_game->GetZoomScopeSize();
	int zoomFactor = the_game->GetZoomWindowFactor();

	// zoom window border
	drawrect(img, zoomWindowPosition.X-2, zoomWindowPosition.Y-2, zoomScopeSize*zoomFactor+2, zoomScopeSize*zoomFactor+2, 192, 192, 192, 255);
	drawrect(img, zoomWindowPosition.X-1, zoomWindowPosition.Y-1, zoomScopeSize*zoomFactor, zoomScopeSize*zoomFactor, 0, 0, 0, 255);
	clearrect(img, zoomWindowPosition.X, zoomWindowPosition.Y, zoomScopeSize*zoomFactor, zoomScopeSize*zoomFactor);

	// zoomed pixels themselves
	for (int j = 0; j < zoomScopeSize; j++)
		for (int i = 0; i < zoomScopeSize; i++)
		{
			pixel pix = img[(j+zoomScopePosition.Y)*(XRES+BARSIZE) + (i+zoomScopePosition.X)];
			for (int y = 0; y < zoomFactor-1; y++)
				for (int x = 0; x < zoomFactor-1; x++)
					img[(j*zoomFactor+y+zoomWindowPosition.Y)*(XRES+BARSIZE) + (i*zoomFactor+x+zoomWindowPosition.X)] = pix;
		}

	// draw box showing where zoom window is
	for (int j = -1; j <= zoomScopeSize; j++)
	{
		xor_pixel(zoomScopePosition.X+j, zoomScopePosition.Y-1, img);
		xor_pixel(zoomScopePosition.X+j, zoomScopePosition.Y+zoomScopeSize, img);
	}
	for (int j = 0; j < zoomScopeSize; j++)
	{
		xor_pixel(zoomScopePosition.X-1, zoomScopePosition.Y+j, img);
		xor_pixel(zoomScopePosition.X+zoomScopeSize, zoomScopePosition.Y+j, img);
	}
}

int render_thumb(void *thumb, int size, int bzip2, pixel *vid_buf, int px, int py, int scl)
{
	unsigned char *d,*c=(unsigned char*)thumb;
	int i,j,x,y,a,t,r,g,b,sx,sy;

	if (bzip2)
	{
		if (size<16)
			return 1;
		if (c[3]!=0x74 || c[2]!=0x49 || c[1]!=0x68 || c[0]!=0x53)
			return 1;
		//if (c[4]>PT_NUM)
		//	return 2;
		if (c[5]!=CELL || c[6]!=XRES/CELL || c[7]!=YRES/CELL)
			return 3;
		i = XRES*YRES;
		d = (unsigned char*)malloc(i);
		if (!d)
			return 1;

		if (BZ2_bzBuffToBuffDecompress((char *)d, (unsigned *)&i, (char *)(c+8), size-8, 0, 0))
			return 1;
		size = i;
	}
	else
		d = c;

	if (size < XRES*YRES)
	{
		if (bzip2)
			free(d);
		return 1;
	}

	sy = 0;
	for (y=0; y+scl<=YRES; y+=scl)
	{
		sx = 0;
		for (x=0; x+scl<=XRES; x+=scl)
		{
			a = 0;
			r = g = b = 0;
			for (j=0; j<scl; j++)
				for (i=0; i<scl; i++)
				{
					t = d[(y+j)*XRES+(x+i)];
					if (t==0xFF)
					{
						r += 256;
						g += 256;
						b += 256;
						a += 2;
					}
					else if (t)
					{
						if (t>=PT_NUM)
							goto corrupt;
						r += COLR(globalSim->elements[t].Colour);
						g += COLG(globalSim->elements[t].Colour);
						b += COLB(globalSim->elements[t].Colour);
						a ++;
					}
				}
			if (a)
			{
				a = 256/a;
				r = (r*a)>>8;
				g = (g*a)>>8;
				b = (b*a)>>8;
			}

			drawpixel(vid_buf, px+sx, py+sy, r, g, b, 255);
			sx++;
		}
		sy++;
	}

	if (bzip2)
		free(d);
	return 0;

corrupt:
	if (bzip2)
		free(d);
	return 1;
}

//draws the cursor
void render_cursor(pixel *vid, int x, int y, Tool* tool, Brush* brush)
{
	int rx = brush->GetRadius().X, ry = brush->GetRadius().Y;
	int i,j;
	if ((sdl_mod & (KMOD_CTRL|KMOD_GUI)) && (sdl_mod & KMOD_SHIFT) && (tool->GetType() != TOOL_TOOL || ((ToolTool*)tool)->GetID() == TOOL_PROP))
	{
		if (tool->GetType() != DECO_TOOL)
		{
			for (i = -5; i < 6; i++)
				if (i != 0)
					xor_pixel(x+i, y, vid);
			for (j = -5; j < 6; j++)
				if (j != 0)
					xor_pixel(x, y+j, vid);
		}
	}
	else if (tool->GetType() == WALL_TOOL)
	{
		int wallx = (x/CELL)*CELL, wally = (y/CELL)*CELL;
		int wallrx = (rx/CELL)*CELL, wallry = (ry/CELL)*CELL;
		xor_line(wallx-wallrx, wally-wallry, wallx+wallrx+CELL-1, wally-wallry, vid);
		xor_line(wallx-wallrx, wally+wallry+CELL-1, wallx+wallrx+CELL-1, wally+wallry+CELL-1, vid);
		xor_line(wallx-wallrx, wally-wallry+1, wallx-wallrx, wally+wallry+CELL-2, vid);
		xor_line(wallx+wallrx+CELL-1, wally-wallry+1, wallx+wallrx+CELL-1, wally+wallry+CELL-2, vid);
	}
	else
	{
		if (rx<=0)
			for (j = y - ry; j <= y + ry; j++)
				xor_pixel(x, j, vid);
		else
		{
			int tempy = y - 1, i, j, oldy;
			if (brush->GetShape() == TRI_BRUSH)
				tempy = y + ry;
			for (i = x - rx; i <= x; i++) {
				oldy = tempy;
				// Find new position just outside of circle boundary
				while (tempy >= y - ry && brush->IsInside(i-x,tempy-y))
					tempy = tempy - 1;
				// Make border 1 pixel thick instead of 2
				if (i != x - rx && oldy != tempy)
					oldy--;
				// Triangle brush is prettier without a complete line drawn between all points
				if (brush->GetShape() == TRI_BRUSH)
					oldy = tempy;
				for (j = tempy + 1; j <= oldy + 1; j++) {
					int i2 = 2*x-i, j2 = 2*y-j;
					if (brush->GetShape() == TRI_BRUSH)
						j2 = y+ry;
					xor_pixel(i, j, vid);
					if (i2 != i)
						xor_pixel(i2, j, vid);
					if (j2 != j)
						xor_pixel(i, j2, vid);
					if (i2 != i && j2 != j)
						xor_pixel(i2, j2, vid);
				}
			}
		}
	}
}

float maxAverage = 0.0f; //for debug mode
int draw_debug_info(pixel* vid, Simulation * sim, int lx, int ly, int cx, int cy, int line_x, int line_y)
{
	char infobuf[256];
	if(debug_flags & DEBUG_LINES)
	{
		if (the_game->GetDrawState() == PowderToy::LINE && the_game->IsMouseDown()) //Line tool
		{
			blend_line(vid, 0, line_y, XRES, line_y, 255, 255, 255, 120);
			blend_line(vid, line_x, 0, line_x, YRES, 255, 255, 255, 120);
	
			blend_line(vid, 0, ly, XRES, ly, 255, 255, 255, 120);
			blend_line(vid, lx, 0, lx, YRES, 255, 255, 255, 120);
			
			sprintf(infobuf, "%d x %d", lx, ly);
			drawtext_outline(vid, lx+(lx>line_x?3:-textwidth(infobuf)-3), ly+(ly<line_y?-10:3), infobuf, 255, 255, 255, 200, 0, 0, 0, 120);
			
			sprintf(infobuf, "%d x %d", line_x, line_y);
			drawtext_outline(vid, line_x+(lx<line_x?3:-textwidth(infobuf)-2), line_y+(ly>line_y?-10:3), infobuf, 255, 255, 255, 200, 0, 0, 0, 120);
			
			sprintf(infobuf, "%d", abs(line_x-lx));
			drawtext_outline(vid, (line_x+lx)/2-textwidth(infobuf)/2, line_y+(ly>line_y?-10:3), infobuf, 255, 255, 255, 200, 0, 0, 0, 120);
			
			sprintf(infobuf, "%d", abs(line_y-ly));
			drawtext_outline(vid, line_x+(lx<line_x?3:-textwidth(infobuf)-2), (line_y+ly)/2-3, infobuf, 255, 255, 255, 200, 0, 0, 0, 120);
		}
	}
	if (debug_flags & DEBUG_ELEMENTPOP)
	{
		int yBottom = YRES-10;
		int xStart = 10;

		std::string maxValString;
		std::string halfValString;
		std::stringstream numtostring;


		int maxVal = 255;
		float scale = 1.0f;
		int bars = 0;
		for (int i = 0; i < PT_NUM; i++)
		{
			if (sim->elements[i].Enabled)
			{
				if (maxVal < sim->elementCount[i])
					maxVal = sim->elementCount[i];
				bars++;
			}
		}
		maxAverage = (maxAverage*(1.0f-0.015f)) + (0.015f*maxVal);
		scale = 255.0f/maxAverage;

		numtostring << maxAverage;
		maxValString = numtostring.str();
		numtostring.str("");
		numtostring << maxAverage/2;
		halfValString = numtostring.str();


		fillrect(vid_buf, xStart-5, yBottom - 263, bars+10+textwidth(maxValString.c_str())+10, 255 + 13, 0, 0, 0, 180);

		bars = 0;
		for (int i = 0; i < PT_NUM; i++)
		{
			if (sim->elements[i].Enabled)
			{
				int count = sim->elementCount[i];
				int barSize = (int)(count * scale - 0.5f);
				int barX = bars;//*2;

				draw_line(vid_buf, xStart+barX, yBottom+3, xStart+barX, yBottom+2, COLR(sim->elements[i].Colour), COLG(sim->elements[i].Colour), COLB(sim->elements[i].Colour), XRES+BARSIZE);
				if (sim->elementCount[i])
				{
					if (barSize > 256)
					{
						barSize = 256;
						blendpixel(vid_buf, xStart+barX, yBottom-barSize-3, COLR(sim->elements[i].Colour), COLG(sim->elements[i].Colour), COLB(sim->elements[i].Colour), 255);
						blendpixel(vid_buf, xStart+barX, yBottom-barSize-5, COLR(sim->elements[i].Colour), COLG(sim->elements[i].Colour), COLB(sim->elements[i].Colour), 255);
						blendpixel(vid_buf, xStart+barX, yBottom-barSize-7, COLR(sim->elements[i].Colour), COLG(sim->elements[i].Colour), COLB(sim->elements[i].Colour), 255);
					}
					else
						draw_line(vid_buf, xStart+barX, yBottom-barSize-3, xStart+barX, yBottom-barSize-2, 255, 255, 255, XRES+BARSIZE);
					draw_line(vid_buf, xStart+barX, yBottom-barSize, xStart+barX, yBottom, COLR(sim->elements[i].Colour), COLG(sim->elements[i].Colour), COLB(sim->elements[i].Colour), XRES+BARSIZE);
				}
				bars++;
			}
		}

		drawtext(vid_buf, xStart + bars + 5, yBottom-5, "0", 255, 255, 255, 255);
		drawtext(vid_buf, xStart + bars + 5, yBottom-132, halfValString.c_str(), 255, 255, 255, 255);
		drawtext(vid_buf, xStart + bars + 5, yBottom-260, maxValString.c_str(), 255, 255, 255, 255);
	}
	if(debug_flags & DEBUG_PARTS)
	{
		int i = 0, x = 0, y = 0, lpx = 0, lpy = 0;
		sprintf(infobuf, "%d/%d (%.2f%%)", sim->parts_lastActiveIndex, NPART, (((float)sim->parts_lastActiveIndex)/((float)NPART))*100.0f);
		for(i = 0; i < NPART; i++){
			if(parts[i].type){
				drawpixel(vid, x, y, 255, 255, 255, 180);
			} else {
				drawpixel(vid, x, y, 0, 0, 0, 180);
			}
			if(i == sim->parts_lastActiveIndex)
			{
				lpx = x;
				lpy = y;
			}
			x++;
			if(x>=XRES){
				y++;
				x = 0;
			}
		}
		draw_line(vid, 0, lpy, XRES, lpy, 0, 255, 120, XRES+BARSIZE);
		draw_line(vid, lpx, 0, lpx, YRES, 0, 255, 120, XRES+BARSIZE);
		drawpixel(vid, lpx, lpy, 255, 50, 50, 220);
				
		drawpixel(vid, lpx+1, lpy, 255, 50, 50, 120);
		drawpixel(vid, lpx-1, lpy, 255, 50, 50, 120);
		drawpixel(vid, lpx, lpy+1, 255, 50, 50, 120);
		drawpixel(vid, lpx, lpy-1, 255, 50, 50, 120);
		
		fillrect(vid, 7, YRES-26, textwidth(infobuf)+5, 14, 0, 0, 0, 180);		
		drawtext(vid, 10, YRES-22, infobuf, 255, 255, 255, 255);
	}
	return 0;
}

void reset_clip_rect()
{
	clip_rect_x1 = 0;
	clip_rect_y1 = 0;
	clip_rect_x2 = VIDXRES;
	clip_rect_y2 = VIDYRES;
}

void set_clip_rect(int x, int y, int w, int h)
{
	clip_rect_x1 = x;
	clip_rect_y1 = y;
	clip_rect_x2 = x + w;
	clip_rect_y2 = y + h;
}
