/// @file TextRenderer.cpp
/// @brief Target independent text rendering (see TextRenderer.hpp).

#include "TextRenderer.hpp"
#include "Font5x7.hpp"

#include <cassert>

//------------------------------------------------------------------
// RenderChar
//------------------------------------------------------------------
void TextRenderer::RenderChar(char c, uint16_t fg, uint16_t bg, int scale, uint16_t* buf)
{
    assert(scale > 0);
    assert(nullptr != buf);

    const uint8_t* glyph = Font5x7::Glyph(c);
    const int width = CHAR_WIDTH * scale;

    for (int row = 0; row < CHAR_HEIGHT; row++)
    {
        for (int col = 0; col < CHAR_WIDTH; col++)
        {
            // The spacing column and row (outside the glyph) are always background
            const bool on = (col < Font5x7::GLYPH_WIDTH) && (row < Font5x7::GLYPH_HEIGHT) &&
                            ((glyph[col] >> row) & 0x01);
            const uint16_t color = on ? fg : bg;

            // Each font pixel becomes a scale x scale block
            for (int dy = 0; dy < scale; dy++)
            {
                uint16_t* out = &buf[((row * scale) + dy) * width + (col * scale)];
                for (int dx = 0; dx < scale; dx++)
                {
                    out[dx] = color;
                }
            }
        }
    }
}
