#include "common/tpt-minmax.h"
#include <algorithm>
#include <cstdlib>
#include "EventLoopSDL.h"
#include "Label.h"
#include "Style.h"
#include "common/Format.h"
#include "common/tpt-math.h"
#include "graphics/VideoBuffer.h"
#include "font.h"
#include "interface/Engine.h"

// TODO: Put label in ui namespace and remove this
using namespace ui;

Label::Label(Point position_, Point size_, std::string text_, bool multiline_, bool noCutoff) :
	Component(position_, size_),
	currentTick(0),
	isClicked(false),
	text(text_),
	textWidth(0),
	textHeight(0),
	multiline(multiline_),
	cursor(0),
	cursorStart(0),
	cursorX(0),
	cursorY(0),
	cursorPosReset(false),
	lastClick(0),
	numClicks(0),
	clickPosition(0),
	noCutoff(noCutoff)
{
	autosizeX = (size.X == AUTOSIZE);
	autosizeY = (size.Y == AUTOSIZE);
	// remove non ascii chars, and newlines for non multiline labels
	text = Format::CleanString(text, true, false, !multiline, false, true);
	UpdateDisplayText();
}

void Label::SetSize(Point size)
{
	Component::SetSize(size);
	if (autosizeX || autosizeY)
		UpdateDisplayText();
}

void Label::SetText(std::string text_)
{
	text = text_;
	text = Format::CleanString(text, true, false, !multiline);
	UpdateDisplayText();
	cursor = cursorStart = text.length();
	cursorPosReset = true;
}

std::string Label::GetText()
{
	std::string fixed = text;
	fixed.erase(std::remove(fixed.begin(), fixed.end(), '\r'), fixed.end()); //strip special newlines
	return fixed;
}

void Label::CopySelection()
{
	int start = (cursor > cursorStart) ? cursorStart : cursor;
	int len = std::abs((int)(cursor-cursorStart));
	std::string copyStr = text.substr(start, len);
	copyStr.erase(std::remove(copyStr.begin(), copyStr.end(), '\r'), copyStr.end()); //strip special newlines
	if (copyStr.length())
		Engine::Ref().ClipboardPush(copyStr);
}

void Label::SelectAll()
{
	cursorStart = 0;
	cursor = clickPosition = text.length();
}

void Label::FindWordPosition(unsigned int position, unsigned int *cursorStart, unsigned int *cursorEnd, const char* spaces)
{
	size_t foundPos = 0, currentPos = 0;
	while (true)
	{
		foundPos = text.find_first_of(spaces, currentPos);
		if (foundPos == text.npos)
		{
			*cursorStart = currentPos;
			*cursorEnd = text.length();
			return;
		}
		else if (foundPos >= position)
		{
			*cursorStart = currentPos;
			*cursorEnd = foundPos;
			return;
		}
		currentPos = foundPos+1;
	}
}

unsigned int Label::UpdateCursor(unsigned int position)
{
	if (numClicks >= 2)
	{
		unsigned int start = 0, end = text.length();
		const char *spaces = " .,!?_():;~\n";
		if (numClicks == 3)
			spaces = "\n";
		FindWordPosition(position, &start, &end, spaces);
		if (start < clickPosition)
		{
			cursorStart = start;
			FindWordPosition(clickPosition, &start, &end, spaces);
			cursor = end;
		}
		else
		{
			cursorStart = end;
			FindWordPosition(clickPosition, &start, &end, spaces);
			cursor = start;
		}
	}
	return position;
}

// updates cursor, cursorX, and cursorY if needed and returns whether cursor position was found (used by UpdateDisplayText)
bool Label::CheckPlaceCursor(bool updateCursor, unsigned int position, int posX, int posY)
{
	// we need to update cursor
	if (updateCursor && ((posX >= cursorX && posY >= cursorY) || (posY >= cursorY+12)))
	{
		cursor = position;
		cursorX = posX-1;
		cursorY = posY;
		return true;
	}
	// don't need to update cursor, but ensure cursorX and Y are right (cursor might have been changed due to typing a letter, etc.)
	else if (!updateCursor && cursor <= position)
	{
		cursorX = posX-1;
		cursorY = posY;
		return true;
	}
	return false;
}

// all-in-one function that updates displayText with \r, updates cursor position, and cuts string where needed
void Label::UpdateDisplayText(bool updateCursor, bool firstClick)
{
	int posX = 0, posY = 12, cursorOffset = 0;
	unsigned int wordStart = 0;
	bool updatedCursor = false;

	//get back the original string by removing the inserted newlines
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());

	// Adjust for icon
	if (icon && !multiline)
		posX += 13;

	updatedCursor = CheckPlaceCursor(updateCursor, 0, posX, posY);
	for (unsigned int i = 0; i < text.length();)
	{
		//find end of word/line chars
		int wordlen = text.substr(i).find_first_of(" .,!?\n");
		if (wordlen == (int)text.npos)
			wordlen = text.length();

		//if a word starts in the last 1/3 of the line, it will get put on the next line if it's too long
		if (wordlen && posX > size.X*2/3)
			wordStart = i;
		else
			wordStart = 0;

		//loop through a word
		for (; --wordlen>=-1 && i < text.length(); i++)
		{
			char c = passwordMask ? 0x8D : text[i];
			switch (c)
			{
			case '\n':
				if (multiline)
				{
					posX = 0;
					posY += 12;
					wordStart = 0;
					//update cursor position (newline check)
					if (!updatedCursor)
						updatedCursor = CheckPlaceCursor(updateCursor, i, posX, posY);
				}
				else
				{
					text = text.substr(0, i);
					text.insert(i, "...");
					i += 3;
				}
				break;
			case '\x0F':
				i += 3;
				wordlen -= 3;
				break;
			case '\b':
				i++;
				wordlen--;
			case '\x0E':
			case '\x01':
			case '\r':
				break;
			case '\xEE':
			{
				char convertedC = Format::ConvertFontIcon(text, i);
				if (convertedC != 0)
				{
					c = convertedC;
					i += 2;
				}
			}
			default:
			{
				bool hasCharacter = posX != 0;
				int charWidth = 0;
				unsigned char uc = static_cast<unsigned char>(c);
				if (uc >= 0x80)
				{
					int seqLen = 0;
					if ((uc & 0xE0) == 0xC0)      seqLen = 2;
					else if ((uc & 0xF0) == 0xE0) seqLen = 3;
					else if ((uc & 0xF8) == 0xF0) seqLen = 4;
					if (seqLen > 0)
					{
						i += seqLen - 1;
						wordlen -= seqLen - 1;
						charWidth = FONT_H;
					}
				}
				else
				{
					charWidth = gfx::VideoBuffer::CharSize(c);
				}
				posX += charWidth;
				if (hasCharacter && !autosizeX && posX+4 >= size.X && !noCutoff)
				{
					if (multiline)
					{
						//if it's too long, figure out where to put the newline (either current position or remembered word start position)
						int replacePos = i;
						if (wordStart)
						{
							replacePos = wordStart;
							i = wordStart;
							// make sure cursor moves back to word start if we inserted the cursor in to this word
							if (updateCursor && updatedCursor && cursor >= wordStart)
								cursor = wordStart-1;
							wordStart = 0;
						}

						//use \r instead of \n, that way it can be easily filtered out when copying without ugly hacks
						text.insert(replacePos, "\r");
						wordlen++;
						if (!updatedCursor)
							cursorOffset++;
						posX = 0;
						posY += 12;
					}
					else
					{
						//for non multiline strings, cut it off here and delete the rest of the string
						text = text.substr(0, i-1 > 0 ? i-1 : 0);
						text.insert(i-1 > 0 ? i-1 : 0, "...");
						i += 2;
					}
				}
				//update cursor position
				if (!updatedCursor)
					updatedCursor = CheckPlaceCursor(updateCursor, i+(posX==0), posX, posY);
				break;
			}
			}
		}
	}
	// update width and height depending on settings
	if (autosizeX)
		size.X = posX+5;
	else if (!multiline)
		textWidth = posX+5;

	if (autosizeY)
		size.Y = posY+5;
	else if (multiline)
		textHeight = posY+3;

	if (updateCursor)
	{
		//if cursor position wasn't found, probably goes at the end
		if (!updatedCursor)
			cursor = text.length();

		//update cursor and cursorStart and adjust for double / triple clicks
		if (firstClick)
			clickPosition = cursorStart = cursor;
		UpdateCursor(cursor);
	}
	else if (!updatedCursor)
	{
		cursorX = posX+1;
		cursorY = posY;
	}
	//make sure cursor isn't out of bounds (for ctrl+a and shift+right)
	if (cursor > text.length())
		cursor = text.length();
	if (cursorStart > text.length())
		cursorStart = text.length();
}

//this function moves the cursor a certain amount, and checks at the end for special characters to move past like \r, \b, and \0F
void Label::MoveCursor(unsigned int *cursor, int amount)
{
	int cur = *cursor, sign = isign<int>(amount), offset = 0;
	if (!amount)
		return;

	offset += amount;
	//adjust for strange characters
	int checkOffset = cur+offset-1, checkOffset2 = cur+offset-3+(sign>0)*2;
	if (checkOffset >= 0 && checkOffset < (int)text.length() && text[checkOffset] == '\b')
		offset += sign;
	else if (checkOffset >= 0 && checkOffset + 1 < (int)text.length() && text[checkOffset + 1] == '\r')
		offset += sign;
	else if (checkOffset2 >= 0 && checkOffset2 < (int)text.length() && text[checkOffset2] == '\x0F') //when moving right, check 1 behind, when moving left, check 3 behind
		offset += sign*3;

	//make sure it's in bounds
	if (cur+offset < 0)
		offset = -cur;
	if (cur+offset > (int)text.length())
		offset = text.length()-cur;

	*cursor += offset;
	cursorPosReset = true;
}

void Label::OnMouseDown(int x, int y, unsigned char button)
{
	if (button == 1)
	{
		numClicks++;
		lastClick = currentTick;

		cursorX = x;
		cursorY = y;
		UpdateDisplayText(true, true);

		if (IsFocused())
			isClicked = true;
	}
}

void Label::OnMouseUp(int x, int y, unsigned char button)
{
	if (IsFocused() && isClicked && button == 1)
	{
		cursorX = x;
		cursorY = y;
		UpdateDisplayText(true);
	}
	isClicked = false;
}

void Label::OnDefocus()
{
	cursorStart = cursor;
}

void Label::OnMouseMoved(int x, int y, Point difference)
{
	if (IsFocused() && isClicked)
	{
		cursorX = x;
		cursorY = y;
		UpdateDisplayText(true);
	}
}

void Label::OnKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	if (ctrl)
	{
		switch (scan)
		{
		case SDL_SCANCODE_C:
		{
			CopySelection();
			break;
		}
		case SDL_SCANCODE_A:
			SelectAll();
			break;
		}
		switch (key)
		{
		case SDLK_LEFT:
		{
			unsigned int start, end;
			FindWordPosition(cursor-1, &start, &end, " .,!?\n");
			if (start < cursor)
				MoveCursor(&cursor, start-cursor-(cursor > cursorStart));
			if (!shift)
				cursorStart = cursor;
			return;
		}
		case SDLK_RIGHT:
		{
			unsigned int start, end;
			FindWordPosition(cursor+1, &start, &end, " .,!?\n");
			MoveCursor(&cursor, end-cursor+!(cursor > cursorStart));
			if (!shift)
				cursorStart = cursor;
			return;
		}
		case SDLK_HOME:
			cursor = 0;
			if (!shift)
				cursorStart = cursor;
			return;
		case SDLK_END:
			cursor = text.length();
			if (!shift)
				cursorStart = cursor;
			return;
		}
	}
	if (shift)
	{
		switch (key)
		{
		case SDLK_LEFT:
			if (cursor)
				MoveCursor(&cursor, -1);
			break;
		case SDLK_RIGHT:
			if (cursor < text.length())
				MoveCursor(&cursor, 1);
			break;
		case SDLK_UP:
			cursor = 0;
			break;
		case SDLK_DOWN:
			cursor = text.length();
			break;
		}
	}

	switch (key)
	{
	case SDLK_HOME:
	{
		unsigned int start, end;
		FindWordPosition(cursor-1, &start, &end, "\r");
		if (start < cursor)
			MoveCursor(&cursor, start-cursor-(cursor > cursorStart));
		if (!shift)
			cursorStart = cursor;
		break;
	}
	case SDLK_END:
	{
		unsigned int start, end;
		FindWordPosition(cursor+1, &start, &end, "\r");
		MoveCursor(&cursor, end-cursor+!(cursor > cursorStart));
		if (!shift)
			cursorStart = cursor;
		break;
	}
	}
}

void Label::OnDraw(gfx::VideoBuffer* vid)
{
	int posX = position.X + 3;
	if (icon && !multiline)
	{
		DrawIcon(icon, vid, { posX, position.Y + 4 });
		posX += 13;
	}

	if (enabled)
	{
		//at some point there will be a redraw variable so this isn't done every frame
		std::string mootext = text;
		if (passwordMask)
			mootext = std::string(text.length(), 0x8D);
		if (cursor != cursorStart || numClicks > 1)
		{
			mootext.insert(cursorStart, "\x01");
			mootext.insert(cursor+(cursor > cursorStart), "\x01");
		}
		if (ShowCursor() && IsFocused())
		{
			mootext.insert(cursor+(cursor > cursorStart)*2, "\x02");
		}
		vid->DrawString(posX, position.Y+4, mootext, color);
	}
	else
		vid->DrawString(posX, position.Y+4, text, COLMULT(color, Style::DisabledMultiplier));
}

void Label::OnTick(uint32_t ticks)
{
	currentTick += ticks;
	if (!IsClicked() && numClicks && lastClick+300 < currentTick)
		numClicks = 0;
}
