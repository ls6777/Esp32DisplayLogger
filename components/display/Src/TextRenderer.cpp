/// @file TextRenderer.cpp
/// @brief Target independent text rendering (see TextRenderer.hpp).

#include "TextRenderer.hpp"
#include "Font5x7.hpp"

#include <cassert>

//------------------------------------------------------------------
// RenderChar
//------------------------------------------------------------------
void TextRenderer::RenderChar(char c, uint16_t fg, uint16_t bg, int scale, uint16_t* buf, Rotation rotation)
{
    assert(scale > 0);
    assert(nullptr != buf);

    const uint8_t* glyph = Font5x7::Glyph(c);
    const int width = CHAR_WIDTH * scale;   // unrotated cell size
    const int height = CHAR_HEIGHT * scale;

    // Walk the unrotated, scaled cell and write each pixel where it lands after the rotation
    for (int y = 0; y < height; y++)
    {
        const int row = y / scale;
        for (int x = 0; x < width; x++)
        {
            const int col = x / scale;

            // The spacing column and row (outside the glyph) are always background
            const bool on = (col < Font5x7::GLYPH_WIDTH) && (row < Font5x7::GLYPH_HEIGHT) &&
                            ((glyph[col] >> row) & 0x01);

            // Clockwise: unrotated (x, y) moves to (height - 1 - y, x) in a cell that is height pixels wide
            const int index = (rotation == Rotation::NONE) ? ((y * width) + x) : ((x * height) + (height - 1 - y));
            buf[index] = on ? fg : bg;
        }
    }
}
