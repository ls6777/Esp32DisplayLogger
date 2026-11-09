/// @file test_TextRenderer.cpp
/// @brief Host unit tests for TextRenderer and Font5x7 (the target independent display logic).

#include "Font5x7.hpp"
#include "TextRenderer.hpp"

#include "unity.h"

#include <cstdio>
#include <cstring>
#include <string>
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

/// @brief Characters with the top bit set (negative when char is signed) are shown as '?'
void test_HighCharactersAreQuestionMark(void)
{
    const uint8_t* question = Font5x7::Glyph('?');
    for (int c = 0x80; c <= 0xFF; c++)
    {
        TEST_ASSERT_EQUAL_PTR(question, Font5x7::Glyph(static_cast<char>(c)));
    }
    for (int c = 0; c < Font5x7::FIRST_CHAR; c++)
    {
        TEST_ASSERT_EQUAL_PTR(question, Font5x7::Glyph(static_cast<char>(c)));
    }
}

/// @brief No two characters have the same glyph (e.g. 0/O, 1/l/I stay distinguishable)
void test_GlyphsAreUnique(void)
{
    for (char a = Font5x7::FIRST_CHAR; a <= Font5x7::LAST_CHAR; a++)
    {
        for (char b = static_cast<char>(a + 1); b <= Font5x7::LAST_CHAR; b++)
        {
            const bool same = (std::memcmp(Font5x7::Glyph(a), Font5x7::Glyph(b), Font5x7::GLYPH_WIDTH) == 0);
            char msg[32];
            std::snprintf(msg, sizeof(msg), "'%c' and '%c' are identical", a, b);
            TEST_ASSERT_FALSE_MESSAGE(same, msg);
        }
    }
}

/// @brief Every character at every scale writes the whole cell and nothing past it
void test_RenderStaysInCell(void)
{
    constexpr uint16_t CANARY = 0x5A5A;     // neither FG nor BG
    constexpr int GUARD = 16;               // extra pixels after the cell that must stay untouched

    for (int scale = 1; scale <= 4; scale++)
    {
        const int cell = TextRenderer::CellPixels(scale);
        for (int c = 0; c <= 0xFF; c++)
        {
            std::vector<uint16_t> buf(cell + GUARD, CANARY);
            TextRenderer::RenderChar(static_cast<char>(c), FG, BG, scale, buf.data());

            for (int i = 0; i < cell; i++)
            {
                TEST_ASSERT_TRUE_MESSAGE((buf[i] == FG) || (buf[i] == BG), "pixel not written");
            }
            for (int i = cell; i < cell + GUARD; i++)
            {
                TEST_ASSERT_EQUAL_HEX16_MESSAGE(CANARY, buf[i], "wrote past the cell");
            }
        }
    }
}

/// @brief The colors passed in are the ones used, and the spacing column and row are always background
void test_ColorsAndSpacing(void)
{
    constexpr uint16_t TEXT = Color::YELLOW;
    constexpr uint16_t BACK = Color::BLUE;

    for (char c = Font5x7::FIRST_CHAR; c <= Font5x7::LAST_CHAR; c++)
    {
        std::vector<uint16_t> buf(TextRenderer::CellPixels(1));
        TextRenderer::RenderChar(c, TEXT, BACK, 1, buf.data());

        for (int y = 0; y < TextRenderer::CHAR_HEIGHT; y++)
        {
            TEST_ASSERT_EQUAL_HEX16(BACK, Pixel(buf, 1, TextRenderer::CHAR_WIDTH - 1, y));
        }
        for (int x = 0; x < TextRenderer::CHAR_WIDTH; x++)
        {
            TEST_ASSERT_EQUAL_HEX16(BACK, Pixel(buf, 1, x, TextRenderer::CHAR_HEIGHT - 1));
        }
        for (const uint16_t px : buf)
        {
            TEST_ASSERT_TRUE((px == TEXT) || (px == BACK));
        }
    }
}

namespace
{
    /// @brief Render a character at scale 1 as ASCII art: '#' for text, '.' for background, one string per row
    /// @param[in] c - character
    /// @return CHAR_HEIGHT rows of CHAR_WIDTH characters
    std::vector<std::string> Art(char c)
    {
        const std::vector<uint16_t> buf = Render(c, 1);
        std::vector<std::string> rows;
        for (int y = 0; y < TextRenderer::CHAR_HEIGHT; y++)
        {
            std::string row;
            for (int x = 0; x < TextRenderer::CHAR_WIDTH; x++)
            {
                row += (Pixel(buf, 1, x, y) == FG) ? '#' : '.';
            }
            rows.push_back(row);
        }
        return rows;
    }

    /// @brief Check that a character renders as the expected ASCII art
    /// @param[in] c - character
    /// @param[in] expected - CHAR_HEIGHT rows of CHAR_WIDTH characters ('#' = text, '.' = background)
    void CheckArt(char c, const std::vector<std::string>& expected)
    {
        const std::vector<std::string> actual = Art(c);
        TEST_ASSERT_EQUAL(expected.size(), actual.size());
        for (size_t row = 0; row < expected.size(); row++)
        {
            char msg[48];
            std::snprintf(msg, sizeof(msg), "'%c' row %d: got %s", c, static_cast<int>(row), actual[row].c_str());
            TEST_ASSERT_EQUAL_STRING_MESSAGE(expected[row].c_str(), actual[row].c_str(), msg);
        }
    }
}

/// @brief The letters of "Hello World!" have the right shapes
void test_Art_HelloWorld(void)
{
    CheckArt('H', {"#...#.",
                   "#...#.",
                   "#...#.",
                   "#####.",
                   "#...#.",
                   "#...#.",
                   "#...#.",
                   "......"});
    CheckArt('e', {"......",
                   "......",
                   ".###..",
                   "#...#.",
                   "#####.",
                   "#.....",
                   ".###..",
                   "......"});
    CheckArt('l', {".##...",
                   "..#...",
                   "..#...",
                   "..#...",
                   "..#...",
                   "..#...",
                   ".###..",
                   "......"});
    CheckArt('o', {"......",
                   "......",
                   ".###..",
                   "#...#.",
                   "#...#.",
                   "#...#.",
                   ".###..",
                   "......"});
    CheckArt('W', {"#...#.",
                   "#...#.",
                   "#...#.",
                   "#.#.#.",
                   "#.#.#.",
                   "#.#.#.",
                   ".#.#..",
                   "......"});
    CheckArt('r', {"......",
                   "......",
                   "#.##..",
                   "##..#.",
                   "#.....",
                   "#.....",
                   "#.....",
                   "......"});
    CheckArt('d', {"....#.",
                   "....#.",
                   ".##.#.",
                   "#..##.",
                   "#...#.",
                   "#...#.",
                   ".####.",
                   "......"});
    CheckArt('!', {"..#...",
                   "..#...",
                   "..#...",
                   "..#...",
                   "..#...",
                   "......",
                   "..#...",
                   "......"});
}

/// @brief Characters that are easy to confuse have the right shapes
void test_Art_LookAlikes(void)
{
    CheckArt('0', {".###..",
                   "#...#.",
                   "#..##.",
                   "#.#.#.",
                   "##..#.",
                   "#...#.",
                   ".###..",
                   "......"});
    CheckArt('O', {".###..",
                   "#...#.",
                   "#...#.",
                   "#...#.",
                   "#...#.",
                   "#...#.",
                   ".###..",
                   "......"});
    CheckArt('1', {"..#...",
                   ".##...",
                   "..#...",
                   "..#...",
                   "..#...",
                   "..#...",
                   ".###..",
                   "......"});
    CheckArt('I', {".###..",
                   "..#...",
                   "..#...",
                   "..#...",
                   "..#...",
                   "..#...",
                   ".###..",
                   "......"});
    CheckArt('?', {".###..",
                   "#...#.",
                   "....#.",
                   "...#..",
                   "..#...",
                   "......",
                   "..#...",
                   "......"});
}

/// @brief Rotated cells swap width and height; the pixel count is the same
void test_CellSize_Rotation(void)
{
    using Rotation = TextRenderer::Rotation;
    for (int scale = 1; scale <= 4; scale++)
    {
        TEST_ASSERT_EQUAL(6 * scale, TextRenderer::CellWidth(scale, Rotation::NONE));
        TEST_ASSERT_EQUAL(8 * scale, TextRenderer::CellHeight(scale, Rotation::NONE));
        TEST_ASSERT_EQUAL(8 * scale, TextRenderer::CellWidth(scale, Rotation::CW90));
        TEST_ASSERT_EQUAL(6 * scale, TextRenderer::CellHeight(scale, Rotation::CW90));
        TEST_ASSERT_EQUAL(TextRenderer::CellPixels(scale),
                          TextRenderer::CellWidth(scale, Rotation::CW90) * TextRenderer::CellHeight(scale, Rotation::CW90));
    }
}

/// @brief Rotated clockwise, 'L' becomes a bar along the top with a stem down the left: the top of the glyph faces
///        right, the spacing row is the left column and the spacing column is the bottom row
void test_Art_RotatedL(void)
{
    constexpr int W = TextRenderer::CHAR_HEIGHT;    // rotated cell is 8 wide, 6 high
    constexpr int H = TextRenderer::CHAR_WIDTH;
    std::vector<uint16_t> buf(TextRenderer::CellPixels(1));
    TextRenderer::RenderChar('L', FG, BG, 1, buf.data(), TextRenderer::Rotation::CW90);

    const char* expected[H] = {".#######",
                               ".#......",
                               ".#......",
                               ".#......",
                               ".#......",
                               "........"};
    for (int y = 0; y < H; y++)
    {
        std::string row;
        for (int x = 0; x < W; x++)
        {
            row += (buf[(y * W) + x] == FG) ? '#' : '.';
        }
        TEST_ASSERT_EQUAL_STRING(expected[y], row.c_str());
    }
}

/// @brief For every character and scale, the rotated cell is exactly the unrotated cell turned 90 degrees clockwise
void test_Rotation_IsClockwiseTurn(void)
{
    for (int scale = 1; scale <= 4; scale++)
    {
        const int w = TextRenderer::CHAR_WIDTH * scale;     // unrotated size
        const int h = TextRenderer::CHAR_HEIGHT * scale;
        for (char c = Font5x7::FIRST_CHAR; c <= Font5x7::LAST_CHAR; c++)
        {
            const std::vector<uint16_t> normal = Render(c, scale);
            std::vector<uint16_t> rotated(TextRenderer::CellPixels(scale), 0x1234);
            TextRenderer::RenderChar(c, FG, BG, scale, rotated.data(), TextRenderer::Rotation::CW90);

            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    // Clockwise: (x, y) in a w x h cell lands at (h - 1 - y, x) in an h x w cell
                    TEST_ASSERT_EQUAL_HEX16(normal[(y * w) + x], rotated[(x * h) + (h - 1 - y)]);
                }
            }
        }
    }
}

/// @brief Rotated rendering also writes the whole cell and nothing past it
void test_RenderRotatedStaysInCell(void)
{
    constexpr uint16_t CANARY = 0x5A5A;
    constexpr int GUARD = 16;

    for (int scale = 1; scale <= 4; scale++)
    {
        const int cell = TextRenderer::CellPixels(scale);
        for (int c = 0; c <= 0xFF; c++)
        {
            std::vector<uint16_t> buf(cell + GUARD, CANARY);
            TextRenderer::RenderChar(static_cast<char>(c), FG, BG, scale, buf.data(), TextRenderer::Rotation::CW90);

            for (int i = 0; i < cell; i++)
            {
                TEST_ASSERT_TRUE_MESSAGE((buf[i] == FG) || (buf[i] == BG), "pixel not written");
            }
            for (int i = cell; i < cell + GUARD; i++)
            {
                TEST_ASSERT_EQUAL_HEX16_MESSAGE(CANARY, buf[i], "wrote past the cell");
            }
        }
    }
}

/// @brief Run all tests
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_GlyphOutOfRange);
    RUN_TEST(test_GlyphsPrintable);
    RUN_TEST(test_HighCharactersAreQuestionMark);
    RUN_TEST(test_GlyphsAreUnique);
    RUN_TEST(test_SpaceIsBackground);
    RUN_TEST(test_RenderH);
    RUN_TEST(test_ScaleReplicatesPixels);
    RUN_TEST(test_RenderStaysInCell);
    RUN_TEST(test_ColorsAndSpacing);
    RUN_TEST(test_Art_HelloWorld);
    RUN_TEST(test_Art_LookAlikes);
    RUN_TEST(test_CellSize_Rotation);
    RUN_TEST(test_Art_RotatedL);
    RUN_TEST(test_Rotation_IsClockwiseTurn);
    RUN_TEST(test_RenderRotatedStaysInCell);
    RUN_TEST(test_Rgb565);
    return UNITY_END();
}
