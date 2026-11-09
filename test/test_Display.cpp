/// @file test_Display.cpp
/// @brief Host unit tests for Display.cpp, using a fake DisplayPort that records every call.

#include "Display.hpp"
#include "DisplayPort.hpp"
#include "TextRenderer.hpp"

#include "unity.h"

#include <string>
#include <vector>

namespace
{
    constexpr int WIDTH = 320;      ///< Width of the fake panel
    constexpr int HEIGHT = 240;     ///< Height of the fake panel

    constexpr uint16_t FG = 0x1234; ///< Text color used by the tests
    constexpr uint16_t BG = 0xABCD; ///< Background color used by the tests

    /// @brief One call made to the fake DisplayPort
    struct Call
    {
        std::string name;               ///< Function called
        int x;                          ///< x argument (FillRect, DrawBitmap)
        int y;                          ///< y argument (FillRect, DrawBitmap)
        int w;                          ///< w argument (FillRect, DrawBitmap)
        int h;                          ///< h argument (FillRect, DrawBitmap)
        uint16_t color;                 ///< color argument (FillRect)
        std::vector<uint16_t> pixels;   ///< Copy of the pixels (DrawBitmap)
    };

    std::vector<Call> calls;    ///< Every call made to the fake DisplayPort, in order

    /// @brief Render a character the way Display should
    /// @param[in] c - character
    /// @param[in] scale - size multiplier
    /// @return the expected pixels
    std::vector<uint16_t> Expected(char c, int scale)
    {
        std::vector<uint16_t> buf(TextRenderer::CellPixels(scale));
        TextRenderer::RenderChar(c, FG, BG, scale, buf.data());
        return buf;
    }

    /// @brief Check that a call is a DrawBitmap of one character
    /// @param[in] call - call to check
    /// @param[in] c - character expected
    /// @param[in] x - expected left edge
    /// @param[in] y - expected top edge
    /// @param[in] scale - expected size multiplier
    void CheckChar(const Call& call, char c, int x, int y, int scale)
    {
        TEST_ASSERT_EQUAL_STRING("DrawBitmap", call.name.c_str());
        TEST_ASSERT_EQUAL(x, call.x);
        TEST_ASSERT_EQUAL(y, call.y);
        TEST_ASSERT_EQUAL(TextRenderer::CHAR_WIDTH * scale, call.w);
        TEST_ASSERT_EQUAL(TextRenderer::CHAR_HEIGHT * scale, call.h);
        const std::vector<uint16_t> expected = Expected(c, scale);
        TEST_ASSERT_EQUAL(expected.size(), call.pixels.size());
        TEST_ASSERT_EQUAL_HEX16_ARRAY(expected.data(), call.pixels.data(), expected.size());
    }
}

//------------------------------------------------------------------
// Fake DisplayPort
//------------------------------------------------------------------
void DisplayPort::Initialize()
{
    calls.push_back(Call{"Initialize", 0, 0, 0, 0, 0, {}});
}

void DisplayPort::BacklightOn()
{
    calls.push_back(Call{"BacklightOn", 0, 0, 0, 0, 0, {}});
}

int DisplayPort::GetWidth()
{
    return WIDTH;
}

int DisplayPort::GetHeight()
{
    return HEIGHT;
}

void DisplayPort::FillRect(int x, int y, int w, int h, uint16_t color)
{
    calls.push_back(Call{"FillRect", x, y, w, h, color, {}});
}

void DisplayPort::DrawBitmap(int x, int y, int w, int h, const uint16_t* pixels)
{
    calls.push_back(Call{"DrawBitmap", x, y, w, h, 0, std::vector<uint16_t>(pixels, pixels + (w * h))});
}

/// @brief Unity: called before each test
void setUp(void)
{
    calls.clear();
}

/// @brief Unity: called after each test
void tearDown(void) {}

//------------------------------------------------------------------
// Tests
//------------------------------------------------------------------

/// @brief Initialize sets up the port, clears the whole screen to black, and only then turns on the backlight
void test_Initialize_ClearsBeforeBacklight(void)
{
    Display::Initialize();

    TEST_ASSERT_EQUAL(3, calls.size());
    TEST_ASSERT_EQUAL_STRING("Initialize", calls[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("FillRect", calls[1].name.c_str());
    TEST_ASSERT_EQUAL(0, calls[1].x);
    TEST_ASSERT_EQUAL(0, calls[1].y);
    TEST_ASSERT_EQUAL(WIDTH, calls[1].w);
    TEST_ASSERT_EQUAL(HEIGHT, calls[1].h);
    TEST_ASSERT_EQUAL_HEX16(Color::BLACK, calls[1].color);
    TEST_ASSERT_EQUAL_STRING("BacklightOn", calls[2].name.c_str());
}

/// @brief The size comes from the port
void test_Size_FromPort(void)
{
    TEST_ASSERT_EQUAL(WIDTH, Display::GetWidth());
    TEST_ASSERT_EQUAL(HEIGHT, Display::GetHeight());
}

/// @brief Clear fills the whole screen with the given color
void test_Clear_FillsScreen(void)
{
    Display::Clear(Color::BLUE);

    TEST_ASSERT_EQUAL(1, calls.size());
    TEST_ASSERT_EQUAL_STRING("FillRect", calls[0].name.c_str());
    TEST_ASSERT_EQUAL(0, calls[0].x);
    TEST_ASSERT_EQUAL(0, calls[0].y);
    TEST_ASSERT_EQUAL(WIDTH, calls[0].w);
    TEST_ASSERT_EQUAL(HEIGHT, calls[0].h);
    TEST_ASSERT_EQUAL_HEX16(Color::BLUE, calls[0].color);
}

/// @brief TextWidth is the number of characters times the scaled cell width
void test_TextWidth(void)
{
    TEST_ASSERT_EQUAL(0, Display::TextWidth("", 1));
    TEST_ASSERT_EQUAL(6, Display::TextWidth("A", 1));
    TEST_ASSERT_EQUAL(36, Display::TextWidth("abc", 2));
    TEST_ASSERT_EQUAL(288, Display::TextWidth("Hello World!", 4));
}

/// @brief Each character is drawn as one cell, left to right, with the right pixels and colors
void test_DrawString_DrawsEachCharacter(void)
{
    Display::DrawString(10, 20, "Hi!", FG, BG, 2);

    TEST_ASSERT_EQUAL(3, calls.size());
    CheckChar(calls[0], 'H', 10, 20, 2);
    CheckChar(calls[1], 'i', 22, 20, 2);
    CheckChar(calls[2], '!', 34, 20, 2);
}

/// @brief All scales from 1 to MAX_SCALE work
void test_DrawString_AllScales(void)
{
    for (int scale = 1; scale <= Display::MAX_SCALE; scale++)
    {
        calls.clear();
        Display::DrawString(0, 0, "Ag", FG, BG, scale);

        TEST_ASSERT_EQUAL(2, calls.size());
        CheckChar(calls[0], 'A', 0, 0, scale);
        CheckChar(calls[1], 'g', TextRenderer::CHAR_WIDTH * scale, 0, scale);
    }
}

/// @brief An empty string draws nothing
void test_DrawString_Empty(void)
{
    Display::DrawString(0, 0, "", FG, BG, 1);
    TEST_ASSERT_EQUAL(0, calls.size());
}

/// @brief Characters that would go past the right edge are not drawn; one that ends exactly on the edge is
void test_DrawString_ClipsAtRightEdge(void)
{
    // 6 pixel cells starting at 302: 302, 308, 314 (ends at 320, fits), 320 (doesn't fit)
    Display::DrawString(302, 0, "ABCD", FG, BG, 1);

    TEST_ASSERT_EQUAL(3, calls.size());
    CheckChar(calls[0], 'A', 302, 0, 1);
    CheckChar(calls[1], 'B', 308, 0, 1);
    CheckChar(calls[2], 'C', 314, 0, 1);
}

/// @brief Text starting past the right edge draws nothing
void test_DrawString_StartsPastRightEdge(void)
{
    Display::DrawString(WIDTH, 0, "A", FG, BG, 1);
    Display::DrawString(WIDTH - 5, 0, "A", FG, BG, 1);
    TEST_ASSERT_EQUAL(0, calls.size());
}

/// @brief Text that ends exactly on the bottom edge is drawn
void test_DrawString_FitsBottomEdge(void)
{
    Display::DrawString(0, HEIGHT - 8, "A", FG, BG, 1);

    TEST_ASSERT_EQUAL(1, calls.size());
    CheckChar(calls[0], 'A', 0, HEIGHT - 8, 1);
}

/// @brief Text that would go past the bottom edge draws nothing
void test_DrawString_PastBottomEdge(void)
{
    Display::DrawString(0, HEIGHT - 7, "A", FG, BG, 1);
    Display::DrawString(0, HEIGHT - 31, "A", FG, BG, 4);
    TEST_ASSERT_EQUAL(0, calls.size());
}

/// @brief Text starting at a negative position draws nothing
void test_DrawString_NegativePosition(void)
{
    Display::DrawString(-1, 0, "A", FG, BG, 1);
    Display::DrawString(0, -1, "A", FG, BG, 1);
    TEST_ASSERT_EQUAL(0, calls.size());
}

/// @brief Characters outside the font are drawn as '?'
void test_DrawString_UnknownCharacter(void)
{
    Display::DrawString(0, 0, "\t", FG, BG, 1);

    TEST_ASSERT_EQUAL(1, calls.size());
    CheckChar(calls[0], '?', 0, 0, 1);
}

/// @brief Run all tests
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Initialize_ClearsBeforeBacklight);
    RUN_TEST(test_Size_FromPort);
    RUN_TEST(test_Clear_FillsScreen);
    RUN_TEST(test_TextWidth);
    RUN_TEST(test_DrawString_DrawsEachCharacter);
    RUN_TEST(test_DrawString_AllScales);
    RUN_TEST(test_DrawString_Empty);
    RUN_TEST(test_DrawString_ClipsAtRightEdge);
    RUN_TEST(test_DrawString_StartsPastRightEdge);
    RUN_TEST(test_DrawString_FitsBottomEdge);
    RUN_TEST(test_DrawString_PastBottomEdge);
    RUN_TEST(test_DrawString_NegativePosition);
    RUN_TEST(test_DrawString_UnknownCharacter);
    return UNITY_END();
}
