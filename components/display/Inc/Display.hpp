#pragma once

/// @file Display.hpp
/// @brief Target independent display API: clear the screen and draw text.
/// @details Uses TextRenderer.hpp to render text and DisplayPort.hpp to send pixels to the panel.

#include "TextRenderer.hpp"

#include <cstdint>

/// @brief Display functions
/// @details Static class: there is only one display.
/// @note Not thread safe. Call from one task only, or protect the calls with a mutex.
class Display
{
    public:

        /// @brief Largest text scale supported by DrawString() (sets the size of the character buffer)
        static constexpr int MAX_SCALE = 4;

        Display() = delete;

        /// @brief Initialize the display hardware, clear the screen and turn on the backlight
        /// @note Must be called once, before any other Display function.
        static void Initialize();

        /// @brief Get the width of the display
        /// @return width, in pixels
        static int GetWidth();

        /// @brief Get the height of the display
        /// @return height, in pixels
        static int GetHeight();

        /// @brief Fill the whole screen with one color
        /// @param[in] color - RGB565 color (see Color in TextRenderer.hpp)
        static void Clear(uint16_t color);

        /// @brief Draw a line of text
        /// @details Characters that don't fit on the screen are not drawn. No line wrapping. Unrotated text runs left
        ///          to right. Text rotated 90 degrees clockwise runs top to bottom, with the tops of the characters
        ///          facing right.
        /// @param[in] x - left edge of the text, in pixels
        /// @param[in] y - top edge of the text, in pixels
        /// @param[in] text - null-terminated text to draw
        /// @param[in] fg - text color
        /// @param[in] bg - background color of the text
        /// @param[in] scale - size multiplier, 1 to MAX_SCALE (1 = 6x8 pixels per character)
        /// @param[in] rotation - orientation of the text
        static void DrawString(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale,
                               TextRenderer::Rotation rotation = TextRenderer::Rotation::NONE);

        /// @brief Draw one character
        /// @details Not drawn if any part of it would be off screen.
        /// @param[in] x - left edge of the character cell, in pixels
        /// @param[in] y - top edge of the character cell, in pixels
        /// @param[in] c - character to draw (characters outside the font are shown as '?')
        /// @param[in] fg - text color
        /// @param[in] bg - background color of the cell
        /// @param[in] scale - size multiplier, 1 to MAX_SCALE (1 = 6x8 pixels)
        /// @param[in] rotation - orientation of the character
        /// @return true if drawn, false if it didn't fit
        static bool DrawChar(int x, int y, char c, uint16_t fg, uint16_t bg, int scale,
                             TextRenderer::Rotation rotation = TextRenderer::Rotation::NONE);

        /// @brief Get the length of a line of text, along the direction it runs
        /// @param[in] text - null-terminated text
        /// @param[in] scale - size multiplier (1 = 6x8 pixels per character)
        /// @return width of unrotated text, or height of rotated text, in pixels
        static int TextWidth(const char* text, int scale);
};
