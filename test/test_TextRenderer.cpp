/// @file test_TextRenderer.cpp
/// @brief Host unit tests for TextRenderer and Font5x7 (the target independent display logic).

#include "Font5x7.hpp"
#include "TextRenderer.hpp"

#include "unity.h"

#include <vector>

namespace
{
    constexpr uint16_t FG = 0xFFFF;     ///< Foreground color used by the tests
    constexpr uint16_t BG = 0x0000;     ///< Background color used by the tests

    /// @brief Render a character and return its pixels
    /// @param[in] c - character to render
    /// @param[in] scale - size multiplier
    /// @return CellPixels(scale) pixels, row by row
    std::vector<uint16_t> Render(char c, int scale)
    {
        std::vector<uint16_t> buf(TextRenderer::CellPixels(scale), 0x1234);
        TextRenderer::RenderChar(c, FG, BG, scale, buf.data());
        return buf;
    }

    /// @brief Get one pixel of a rendered character
    /// @param[in] buf - rendered character
    /// @param[in] scale - scale it was rendered at
    /// @param[in] x - column, in pixels
    /// @param[in] y - row, in pixels
    /// @return the pixel's color
    uint16_t Pixel(const std::vector<uint16_t>& buf, int scale, int x, int y)
    {
        return buf[y * TextRenderer::CHAR_WIDTH * scale + x];
    }
}

/// @brief Unity: called before each test
void setUp(void) {}

/// @brief Unity: called after each test
void tearDown(void) {}

/// @brief Characters outside the font are shown as '?'
void test_GlyphOutOfRange(void)
{
    TEST_ASSERT_EQUAL_PTR(Font5x7::Glyph('?'), Font5x7::Glyph('\n'));
    TEST_ASSERT_EQUAL_PTR(Font5x7::Glyph('?'), Font5x7::Glyph(static_cast<char>(0x7F)));
    TEST_ASSERT_NOT_EQUAL(Font5x7::Glyph('?'), Font5x7::Glyph('A'));
}

/// @brief Every printable character except the space has at least one pixel set, and no glyph uses bit 7
void test_GlyphsPrintable(void)
{
    for (char c = Font5x7::FIRST_CHAR; c <= Font5x7::LAST_CHAR; c++)
    {
        const uint8_t* glyph = Font5x7::Glyph(c);
        uint8_t all = 0;
        for (int col = 0; col < Font5x7::GLYPH_WIDTH; col++)
        {
            all |= glyph[col];
        }
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, all & 0x80, "bit 7 used");
        if (c != ' ')
        {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(0, all, "empty glyph");
        }
    }
}

/// @brief A space renders as all background, filling the whole cell
void test_SpaceIsBackground(void)
{
    const std::vector<uint16_t> buf = Render(' ', 1);
    for (const uint16_t px : buf)
    {
        TEST_ASSERT_EQUAL_HEX16(BG, px);
    }
}

/// @brief 'H' at scale 1: full left and right strokes, a crossbar, and blank spacing column and row
void test_RenderH(void)
{
    const std::vector<uint16_t> buf = Render('H', 1);

    for (int y = 0; y < Font5x7::GLYPH_HEIGHT; y++)
    {
        TEST_ASSERT_EQUAL_HEX16(FG, Pixel(buf, 1, 0, y));   // left stroke
        TEST_ASSERT_EQUAL_HEX16(FG, Pixel(buf, 1, 4, y));   // right stroke
        TEST_ASSERT_EQUAL_HEX16(BG, Pixel(buf, 1, 5, y));   // spacing column
    }
    for (int x = 0; x < Font5x7::GLYPH_WIDTH; x++)
    {
        TEST_ASSERT_EQUAL_HEX16(FG, Pixel(buf, 1, x, 3));   // crossbar
        TEST_ASSERT_EQUAL_HEX16(BG, Pixel(buf, 1, x, 7));   // spacing row
    }
    TEST_ASSERT_EQUAL_HEX16(BG, Pixel(buf, 1, 2, 0));       // gap above the crossbar
}

/// @brief At scale 3, every font pixel becomes a 3x3 block of the same color
void test_ScaleReplicatesPixels(void)
{
    constexpr int SCALE = 3;
    const std::vector<uint16_t> one = Render('e', 1);
    const std::vector<uint16_t> big = Render('e', SCALE);

    TEST_ASSERT_EQUAL(TextRenderer::CellPixels(SCALE), static_cast<int>(big.size()));
    for (int y = 0; y < TextRenderer::CHAR_HEIGHT * SCALE; y++)
    {
        for (int x = 0; x < TextRenderer::CHAR_WIDTH * SCALE; x++)
        {
            TEST_ASSERT_EQUAL_HEX16(Pixel(one, 1, x / SCALE, y / SCALE), Pixel(big, SCALE, x, y));
        }
    }
}

/// @brief Rgb565 packs 5-6-5 bits, and the color constants match it
void test_Rgb565(void)
{
    TEST_ASSERT_EQUAL_HEX16(Color::RED, Color::Rgb565(255, 0, 0));
    TEST_ASSERT_EQUAL_HEX16(Color::GREEN, Color::Rgb565(0, 255, 0));
    TEST_ASSERT_EQUAL_HEX16(Color::BLUE, Color::Rgb565(0, 0, 255));
    TEST_ASSERT_EQUAL_HEX16(Color::YELLOW, Color::Rgb565(255, 255, 0));
    TEST_ASSERT_EQUAL_HEX16(Color::WHITE, Color::Rgb565(255, 255, 255));
    TEST_ASSERT_EQUAL_HEX16(Color::BLACK, Color::Rgb565(0, 0, 0));
    TEST_ASSERT_EQUAL_HEX16(0x8410, Color::Rgb565(128, 128, 128));
}

/// @brief Run all tests
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_GlyphOutOfRange);
    RUN_TEST(test_GlyphsPrintable);
    RUN_TEST(test_SpaceIsBackground);
    RUN_TEST(test_RenderH);
    RUN_TEST(test_ScaleReplicatesPixels);
    RUN_TEST(test_Rgb565);
    return UNITY_END();
}
