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
        /// @details Characters that don't fit on the screen are not drawn. No line wrapping.
        /// @param[in] x - left edge of the text, in pixels
        /// @param[in] y - top edge of the text, in pixels
        /// @param[in] text - null-terminated text to draw
        /// @param[in] fg - text color
        /// @param[in] bg - background color of the text
        /// @param[in] scale - size multiplier, 1 to MAX_SCALE (1 = 6x8 pixels per character)
        static void DrawString(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale);

        /// @brief Get the size of a line of text
        /// @param[in] text - null-terminated text
        /// @param[in] scale - size multiplier (1 = 6x8 pixels per character)
        /// @return width of the text, in pixels (the height is TextRenderer::CHAR_HEIGHT * scale)
        static int TextWidth(const char* text, int scale);
};
