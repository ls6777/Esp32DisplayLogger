/// @file Display.cpp
/// @brief Target independent display API (see Display.hpp).

#include "Display.hpp"
#include "DisplayPort.hpp"

#include <cassert>
#include <cstring>

namespace
{
    /// @brief Pixels of one character cell, at up to Display::MAX_SCALE
    uint16_t charBuf[TextRenderer::CellPixels(Display::MAX_SCALE)];
}

//------------------------------------------------------------------
// Initialize
//------------------------------------------------------------------
void Display::Initialize()
{
    DisplayPort::Initialize();
    Clear(Color::BLACK);
    DisplayPort::BacklightOn();
}

//------------------------------------------------------------------
// GetWidth
//------------------------------------------------------------------
int Display::GetWidth()
{
    return DisplayPort::GetWidth();
}

//------------------------------------------------------------------
// GetHeight
//------------------------------------------------------------------
int Display::GetHeight()
{
    return DisplayPort::GetHeight();
}

//------------------------------------------------------------------
// Clear
//------------------------------------------------------------------
void Display::Clear(uint16_t color)
{
    DisplayPort::FillRect(0, 0, DisplayPort::GetWidth(), DisplayPort::GetHeight(), color);
}

//------------------------------------------------------------------
// TextWidth
//------------------------------------------------------------------
int Display::TextWidth(const char* text, int scale)
{
    return static_cast<int>(std::strlen(text)) * TextRenderer::CHAR_WIDTH * scale;
}

//------------------------------------------------------------------
// DrawChar
//------------------------------------------------------------------
bool Display::DrawChar(int x, int y, char c, uint16_t fg, uint16_t bg, int scale, TextRenderer::Rotation rotation)
{
    assert((scale > 0) && (scale <= MAX_SCALE));

    const int cellWidth = TextRenderer::CellWidth(scale, rotation);
    const int cellHeight = TextRenderer::CellHeight(scale, rotation);

    if ((x < 0) || (y < 0) || ((x + cellWidth) > DisplayPort::GetWidth()) ||
        ((y + cellHeight) > DisplayPort::GetHeight()))
    {
        return false;
    }

    TextRenderer::RenderChar(c, fg, bg, scale, charBuf, rotation);
    DisplayPort::DrawBitmap(x, y, cellWidth, cellHeight, charBuf);
    return true;
}

//------------------------------------------------------------------
// DrawString
//------------------------------------------------------------------
void Display::DrawString(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale,
                         TextRenderer::Rotation rotation)
{
    assert(nullptr != text);
    assert((scale > 0) && (scale <= MAX_SCALE));

    const int cellWidth = TextRenderer::CellWidth(scale, rotation);
    const int cellHeight = TextRenderer::CellHeight(scale, rotation);
    const bool rotated = (rotation != TextRenderer::Rotation::NONE);

    for (const char* c = text; *c != '\0'; c++)
    {
        // Stop at the first character that doesn't fit
        if (!DrawChar(x, y, *c, fg, bg, scale, rotation))
        {
            break;
        }

        // Unrotated text runs left to right, rotated text top to bottom
        if (rotated)
        {
            y += cellHeight;
        }
        else
        {
            x += cellWidth;
        }
    }
}
