#pragma once

/// @file Font5x7.hpp
/// @brief Target independent 5x7 pixel bitmap font for printable ASCII.

#include <cstdint>

/// @brief 5x7 pixel bitmap font covering printable ASCII (' ' to '~')
/// @details Static class: there is only one font table. Each glyph is 5 bytes, one per column from left to right.
///          Bit 0 of each byte is the top row and bit 6 the bottom row (bit 7 is unused).
class Font5x7
{
    public:

        static constexpr int GLYPH_WIDTH = 5;   ///< Width of a glyph, in pixels (bytes per glyph)
        static constexpr int GLYPH_HEIGHT = 7;  ///< Height of a glyph, in pixels
        static constexpr char FIRST_CHAR = ' '; ///< First character in the font
        static constexpr char LAST_CHAR = '~';  ///< Last character in the font

        Font5x7() = delete;

        /// @brief Get the bitmap of a character
        /// @param[in] c - character to look up. Characters outside the font are shown as '?'.
        /// @return pointer to the GLYPH_WIDTH column bytes of the glyph
        static const uint8_t* Glyph(char c);
};
