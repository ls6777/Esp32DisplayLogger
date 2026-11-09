#pragma once

/// @file TextRenderer.hpp
/// @brief Target independent rendering of text into RGB565 pixel buffers.
/// @details No OS or display specifics are used here, so the logic can be unit tested on a host machine.

#include <cstdint>

/// @brief RGB565 colors
/// @details Static class: only constants and a conversion helper.
class Color
{
    public:

        Color() = delete;

        /// @brief Convert 8-bit red, green and blue to an RGB565 color
        /// @param[in] r - red (0-255)
        /// @param[in] g - green (0-255)
        /// @param[in] b - blue (0-255)
        /// @return RGB565 color
        static constexpr uint16_t Rgb565(uint8_t r, uint8_t g, uint8_t b)
        {
            return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }

        static constexpr uint16_t BLACK = 0x0000;   ///< Black
        static constexpr uint16_t WHITE = 0xFFFF;   ///< White
        static constexpr uint16_t RED = 0xF800;     ///< Red
        static constexpr uint16_t GREEN = 0x07E0;   ///< Green
        static constexpr uint16_t BLUE = 0x001F;    ///< Blue
        static constexpr uint16_t YELLOW = 0xFFE0;  ///< Yellow
};

/// @brief Renders characters of the 5x7 font (Font5x7.hpp) into RGB565 pixel buffers
/// @details Static class: holds no state. Each character fills a cell of CHAR_WIDTH x CHAR_HEIGHT pixels times the
///          scale: the 5x7 glyph in the top left, plus one blank column on the right and one blank row at the bottom
///          to separate characters and lines.
class TextRenderer
{
    public:

        static constexpr int CHAR_WIDTH = 6;    ///< Width of a character cell at scale 1, in pixels
        static constexpr int CHAR_HEIGHT = 8;   ///< Height of a character cell at scale 1, in pixels

        TextRenderer() = delete;

        /// @brief Get the number of pixels in a character cell
        /// @param[in] scale - size multiplier (1 = 6x8 pixels)
        /// @return pixels needed in the buffer passed to RenderChar()
        static constexpr int CellPixels(int scale)
        {
            return (CHAR_WIDTH * scale) * (CHAR_HEIGHT * scale);
        }

        /// @brief Render one character cell into a pixel buffer
        /// @param[in] c - character to render. Characters outside the font are shown as '?'.
        /// @param[in] fg - color of the character
        /// @param[in] bg - color of the rest of the cell
        /// @param[in] scale - size multiplier (1 = 6x8 pixels). Must be > 0.
        /// @param[out] buf - buffer of at least CellPixels(scale) pixels, filled row by row, left to right
        static void RenderChar(char c, uint16_t fg, uint16_t bg, int scale, uint16_t* buf);
};
