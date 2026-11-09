/// @file test_TextConsole.cpp
/// @brief Host unit tests for TextConsole (the target independent scrolling text console).

#include "TextConsole.hpp"

#include "unity.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    using Small = TextConsole<5, 3>;    ///< Small console used by most tests: 5 columns, 3 lines

    /// @brief One cell reported by Render()
    struct Cell
    {
        int row;    ///< Line
        int col;    ///< Column
        char c;     ///< Character
    };

    /// @brief Add a C string to a console
    /// @tparam C - console type
    /// @param[in,out] console - console to write to
    /// @param[in] text - text to add
    template <typename C>
    void Write(C& console, const char* text)
    {
        console.Write(text, static_cast<int>(std::strlen(text)));
    }

    /// @brief Get one line of a console as a string (trailing spaces kept)
    /// @tparam C - console type
    /// @param[in] console - console to read
    /// @param[in] row - line
    /// @return the line's characters
    template <typename C>
    std::string Line(const C& console, int row)
    {
        std::string line;
        for (int c = 0; c < C::COLS; c++)
        {
            line += console.At(row, c);
        }
        return line;
    }

    /// @brief Render a console and collect the cells it reports
    /// @tparam C - console type
    /// @param[in,out] console - console to render
    /// @return the cells reported, in order
    template <typename C>
    std::vector<Cell> Render(C& console)
    {
        std::vector<Cell> cells;
        const int count = console.Render([&cells](int row, int col, char c) { cells.push_back(Cell{row, col, c}); });
        TEST_ASSERT_EQUAL(static_cast<int>(cells.size()), count);
        return cells;
    }

    /// @brief Check all lines of a small console
    /// @param[in] console - console to check
    /// @param[in] l0 - expected first line
    /// @param[in] l1 - expected second line
    /// @param[in] l2 - expected third line
    void CheckLines(const Small& console, const char* l0, const char* l1, const char* l2)
    {
        TEST_ASSERT_EQUAL_STRING(l0, Line(console, 0).c_str());
        TEST_ASSERT_EQUAL_STRING(l1, Line(console, 1).c_str());
        TEST_ASSERT_EQUAL_STRING(l2, Line(console, 2).c_str());
    }
}

/// @brief Unity: called before each test
void setUp(void) {}

/// @brief Unity: called after each test
void tearDown(void) {}

/// @brief A new console is all spaces, which matches a cleared display, so there's nothing to draw
void test_Empty(void)
{
    Small console;
    CheckLines(console, "     ", "     ", "     ");
    TEST_ASSERT_EQUAL(0, Render(console).size());
}

/// @brief Text fills the first line from the left
void test_Write_FillsFirstLine(void)
{
    Small console;
    Write(console, "abc");
    CheckLines(console, "abc  ", "     ", "     ");
}

/// @brief Render reports each changed cell once, in order; nothing the second time
void test_Render_ReportsChangedCellsOnce(void)
{
    Small console;
    Write(console, "ab");

    const std::vector<Cell> cells = Render(console);
    TEST_ASSERT_EQUAL(2, cells.size());
    TEST_ASSERT_EQUAL(0, cells[0].row);
    TEST_ASSERT_EQUAL(0, cells[0].col);
    TEST_ASSERT_EQUAL_CHAR('a', cells[0].c);
    TEST_ASSERT_EQUAL(0, cells[1].row);
    TEST_ASSERT_EQUAL(1, cells[1].col);
    TEST_ASSERT_EQUAL_CHAR('b', cells[1].c);

    TEST_ASSERT_EQUAL(0, Render(console).size());

    Write(console, "c");
    const std::vector<Cell> more = Render(console);
    TEST_ASSERT_EQUAL(1, more.size());
    TEST_ASSERT_EQUAL(2, more[0].col);
}

/// @brief Writing the same character over itself doesn't need a redraw
void test_Render_SameCharacterNotReported(void)
{
    Small console;
    Write(console, "ab");
    Render(console);
    Write(console, "\rab");     // overwrite with the same text
    TEST_ASSERT_EQUAL(0, Render(console).size());
}

/// @brief Each line (\r\n, as printf sends) goes on the next line down
void test_Lines_GoDown(void)
{
    Small console;
    Write(console, "one\r\ntwo\r\n");
    CheckLines(console, "one  ", "two  ", "     ");
}

/// @brief A bare \n works the same as \r\n
void test_BareNewline(void)
{
    Small console;
    Write(console, "one\ntwo");
    CheckLines(console, "one  ", "two  ", "     ");
}

/// @brief \r returns to the start of the line, so the line is overwritten
void test_CarriageReturn_Overwrites(void)
{
    Small console;
    Write(console, "abc\rX");
    CheckLines(console, "Xbc  ", "     ", "     ");
}

/// @brief Two newlines in a row leave a blank line
void test_BlankLine(void)
{
    Small console;
    Write(console, "a\r\n\r\nb");
    CheckLines(console, "a    ", "     ", "b    ");
}

/// @brief Long lines wrap onto the next line
void test_LongLine_Wraps(void)
{
    Small console;
    Write(console, "abcdefg");
    CheckLines(console, "abcde", "fg   ", "     ");
}

/// @brief A line that exactly fills the width, followed by a newline, doesn't leave a blank line
void test_FullLine_NoBlankLineAfter(void)
{
    Small console;
    Write(console, "abcde\r\nf");
    CheckLines(console, "abcde", "f    ", "     ");
}

/// @brief Once full, the next line goes on the bottom and the rest scroll up
void test_Full_ScrollsUp(void)
{
    Small console;
    Write(console, "L1\r\nL2\r\nL3\r\n");
    CheckLines(console, "L1   ", "L2   ", "L3   ");    // the newest line stays on the bottom, no blank line under it

    Write(console, "L4\r\n");
    CheckLines(console, "L2   ", "L3   ", "L4   ");

    Write(console, "L5\r\n");
    CheckLines(console, "L3   ", "L4   ", "L5   ");
}

/// @brief Scrolling clears the bottom line before the new line is written on it
void test_Scroll_ClearsBottomLine(void)
{
    Small console;
    Write(console, "aaaaa\r\nbbbbb\r\nccccc\r\nd");
    CheckLines(console, "bbbbb", "ccccc", "d    ");
}

/// @brief After a scroll only the cells that changed are reported, so similar lines are cheap to scroll
void test_Scroll_ReportsOnlyChangedCells(void)
{
    Small console;
    Write(console, "a1\r\na2\r\na3\r\n");
    Render(console);

    Write(console, "a4\r\n");                   // lines become a2, a3, a4: only the digits change
    const std::vector<Cell> cells = Render(console);
    TEST_ASSERT_EQUAL(3, cells.size());
    for (int i = 0; i < 3; i++)
    {
        TEST_ASSERT_EQUAL(i, cells[i].row);
        TEST_ASSERT_EQUAL(1, cells[i].col);
        TEST_ASSERT_EQUAL_CHAR('2' + i, cells[i].c);
    }
}

/// @brief Wrapping on the last line scrolls too
void test_Wrap_OnLastLine_Scrolls(void)
{
    Small console;
    Write(console, "aa\r\nbb\r\nccccc" "dd");
    CheckLines(console, "bb   ", "ccccc", "dd   ");
}

/// @brief Many lines: only the last ones remain, in order
void test_ManyLines(void)
{
    TextConsole<8, 4> console;
    char line[16];
    for (int i = 0; i < 100; i++)
    {
        std::snprintf(line, sizeof(line), "n=%d\r\n", i);
        Write(console, line);
    }
    TEST_ASSERT_EQUAL_STRING("n=96    ", Line(console, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("n=97    ", Line(console, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("n=98    ", Line(console, 2).c_str());
    TEST_ASSERT_EQUAL_STRING("n=99    ", Line(console, 3).c_str());
}

/// @brief Text can arrive in pieces, split anywhere (even inside \r\n)
void test_SplitWrites(void)
{
    Small console;
    Write(console, "o");
    Write(console, "ne\r");
    Write(console, "\nt");
    Write(console, "wo");
    CheckLines(console, "one  ", "two  ", "     ");
}

/// @brief ANSI escape sequences (log colors) are not shown, even when split across writes
void test_EscapeSequences_Ignored(void)
{
    TextConsole<20, 2> console;
    Write(console, "\x1b[0;32mI (12) x\x1b[0m\r\n");
    Write(console, "\x1b[");
    Write(console, "1;31");
    Write(console, "mE\x1b" "Xy");             // also a two-byte escape (ESC X)
    TEST_ASSERT_EQUAL_STRING("I (12) x            ", Line(console, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("Ey                  ", Line(console, 1).c_str());
}

/// @brief Tabs become a space; other control characters and DEL are ignored; bytes 0x80+ are kept (shown as '?')
void test_ControlCharacters(void)
{
    Small console;
    const char text[] = {'a', '\t', 'b', '\a', '\b', 0x7F, static_cast<char>(0xE9), 0};
    Write(console, text);
    TEST_ASSERT_EQUAL_CHAR('a', console.At(0, 0));
    TEST_ASSERT_EQUAL_CHAR(' ', console.At(0, 1));
    TEST_ASSERT_EQUAL_CHAR('b', console.At(0, 2));
    TEST_ASSERT_EQUAL_CHAR(static_cast<char>(0xE9), console.At(0, 3));
    TEST_ASSERT_EQUAL_CHAR(' ', console.At(0, 4));
}

/// @brief Run all tests
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Empty);
    RUN_TEST(test_Write_FillsFirstLine);
    RUN_TEST(test_Render_ReportsChangedCellsOnce);
    RUN_TEST(test_Render_SameCharacterNotReported);
    RUN_TEST(test_Lines_GoDown);
    RUN_TEST(test_BareNewline);
    RUN_TEST(test_CarriageReturn_Overwrites);
    RUN_TEST(test_BlankLine);
    RUN_TEST(test_LongLine_Wraps);
    RUN_TEST(test_FullLine_NoBlankLineAfter);
    RUN_TEST(test_Full_ScrollsUp);
    RUN_TEST(test_Scroll_ClearsBottomLine);
    RUN_TEST(test_Scroll_ReportsOnlyChangedCells);
    RUN_TEST(test_Wrap_OnLastLine_Scrolls);
    RUN_TEST(test_ManyLines);
    RUN_TEST(test_SplitWrites);
    RUN_TEST(test_EscapeSequences_Ignored);
    RUN_TEST(test_ControlCharacters);
    return UNITY_END();
}
