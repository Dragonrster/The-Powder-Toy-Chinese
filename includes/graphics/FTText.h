#ifndef FTTEXT_H
#define FTTEXT_H

#include "defines.h"
#include "Pixel.h"
#include <string>

// Check if a string contains non-ASCII (UTF-8 multi-byte) characters that
// require FreeType rendering rather than the built-in bitmap font.
bool FT_HasUnicode(const char *s);

// Initialize FreeType and load a TrueType font from the given path.
// fontPath: path to a .ttf font file.
// fontSize: height in pixels (default 12 for readability of CJK glyphs).
// Returns true on success.
bool FT_Init(const char *fontPath, int fontSize);

// Shutdown FreeType and release resources.
void FT_Shutdown();

// Check if FreeType is initialized and ready.
bool FT_IsReady();

// Pre-process a string: convert \xEE font-icon escape sequences to single-byte
// bitmap-font glyph indices. Callers should use this before passing text to
// FT_DrawText so that icons are rendered via the bitmap font fallback path.
std::string FT_ConvertIcons(const std::string &s);

// Draw a UTF-8 encoded string to a pixel buffer. Supports the same inline
// formatting codes as drawtext(): \n, \bX colour codes, \x0F RGB, \x0E reset,
// \x01 highlight. Does NOT support \xEE font icons (those are bitmap-font
// specific).
// Returns the x-position after the last drawn character.
int FT_DrawText(pixel *vid, int x, int y, const char *s, int r, int g, int b, int a, bool noColor = false);

// Measure the pixel width of a UTF-8 string using the loaded FreeType font.
int FT_TextWidth(const char *s);

// Measure the pixel height of the loaded FreeType font.
int FT_TextHeight();

#endif
