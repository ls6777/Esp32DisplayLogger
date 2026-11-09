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
// DrawString
//------------------------------------------------------------------
void Display::DrawString(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale)
{
    assert(nullptr != text);
    assert((scale > 0) && (scale <= MAX_SCALE));

    const int charWidth = TextRenderer::CHAR_WIDTH * scale;
    const int charHeight = TextRenderer::CHAR_HEIGHT * scale;

    // Nothing can be drawn if the line starts off screen
    if ((x < 0) || (y < 0) || ((y + charHeight) > DisplayPort::GetHeight()))
    {
        return;
    }

    for (const char* c = text; *c != '\0'; c++)
    {
        if ((x + charWidth) > DisplayPort::GetWidth())
        {
            break;
        }

        TextRenderer::RenderChar(*c, fg, bg, scale, charBuf);
        DisplayPort::DrawBitmap(x, y, charWidth, charHeight, charBuf);
        x += charWidth;
    }
}
