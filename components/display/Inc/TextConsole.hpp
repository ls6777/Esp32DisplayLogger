#pragma once

/// @file TextConsole.hpp
/// @brief Target independent scrolling text console (the text side of a terminal).
/// @details Keeps a grid of characters like a terminal screen: text fills each line left to right, long lines wrap,
///          and once the screen is full every new line scrolls the rest up. It knows nothing about pixels: Render()
///          reports each cell that changed since the last render, so only those are redrawn. No OS or display
///          specifics, so it can be unit tested on a host machine.

/// @brief Scrolling text console of Cols x Rows characters
/// @tparam Cols - characters per line
/// @tparam Rows - number of lines
///
/// @details Characters handled by Write():
///          - printable ASCII (and bytes 0x80-0xFF, which the font shows as '?'): written at the cursor
///          - '\\n': ends the line. The move to the next line is deferred until the next character arrives, so the
///            newest line stays on the bottom row once the screen is full (no blank line under it).
///          - '\\r': back to the start of the line
///          - '\\t': written as a space
///          - ANSI escape sequences (e.g. log colors, ESC [ ... m): ignored
///          - other control characters: ignored
template <int Cols, int Rows>
class TextConsole
{
    static_assert(Cols > 0, "TextConsole needs at least one column");
    static_assert(Rows > 0, "TextConsole needs at least one row");

    public:

        static constexpr int COLS = Cols;   ///< Characters per line
        static constexpr int ROWS = Rows;   ///< Number of lines

        /// @brief Constructor: an empty screen (all spaces), matching a cleared display
        TextConsole()
        {
            for (int r = 0; r < Rows; r++)
            {
                for (int c = 0; c < Cols; c++)
                {
                    cells[r][c] = ' ';
                    shown[r][c] = ' ';
                }
            }
        }

        /// @brief Add text to the console
        /// @param[in] data - text to add
        /// @param[in] len - length of text, in bytes
        void Write(const char* data, int len)
        {
            for (int i = 0; i < len; i++)
            {
                Put(data[i]);
            }
        }

        /// @brief Report every cell that changed since the last call, so it can be redrawn
        /// @tparam DrawFn - any callable that can be called as draw(int row, int col, char c)
        /// @param[in] draw - called for each changed cell, row by row, left to right
        /// @return number of cells reported
        template <typename DrawFn>
        int Render(DrawFn&& draw)
        {
            int count = 0;
            for (int r = 0; r < Rows; r++)
            {
                for (int c = 0; c < Cols; c++)
                {
                    if (cells[r][c] != shown[r][c])
                    {
                        draw(r, c, cells[r][c]);
                        shown[r][c] = cells[r][c];
                        count++;
                    }
                }
            }
            return count;
        }

        /// @brief Get the character in a cell
        /// @param[in] row - line, 0 (top) to Rows - 1
        /// @param[in] col - column, 0 (left) to Cols - 1
        /// @return the character (' ' if empty)
        char At(int row, int col) const
        {
            return cells[row][col];
        }

    private:

        /// @brief Escape sequence parsing state
        enum class Escape
        {
            NONE,       ///< Not in an escape sequence
            START,      ///< Got ESC, waiting for the next byte
            CSI         ///< Got ESC [, waiting for the final byte (0x40-0x7E)
        };

        static constexpr char ESC = 0x1B;   ///< Escape character, starts an ANSI escape sequence

        /// @brief Handle one character
        /// @param[in] ch - character
        void Put(char ch)
        {
            const unsigned char c = static_cast<unsigned char>(ch);

            if (escape == Escape::START)
            {
                escape = (c == '[') ? Escape::CSI : Escape::NONE;
                return;
            }
            if (escape == Escape::CSI)
            {
                if ((c >= 0x40) && (c <= 0x7E))
                {
                    escape = Escape::NONE;
                }
                return;
            }

            if (ch == ESC)
            {
                escape = Escape::START;
            }
            else if (ch == '\n')
            {
                // A second '\n' in a row makes a blank line: do the first one's move now, defer this one
                if (newlinePending)
                {
                    NewLine();
                }
                newlinePending = true;
            }
            else if (ch == '\r')
            {
                col = 0;
            }
            else if ((ch == '\t') || (c >= ' '))
            {
                if (c == 0x7F)
                {
                    return;     // DEL is a control character
                }
                if (newlinePending || (col >= Cols))
                {
                    NewLine();
                }
                cells[row][col] = (ch == '\t') ? ' ' : ch;
                col++;
            }
        }

        /// @brief Move to the start of the next line, scrolling everything up one line if on the last line
        void NewLine()
        {
            newlinePending = false;
            col = 0;

            if (row < (Rows - 1))
            {
                row++;
                return;
            }

            for (int r = 0; r < (Rows - 1); r++)
            {
                for (int c = 0; c < Cols; c++)
                {
                    cells[r][c] = cells[r + 1][c];
                }
            }
            for (int c = 0; c < Cols; c++)
            {
                cells[Rows - 1][c] = ' ';
            }
        }

        char cells[Rows][Cols];         ///< Current text
        char shown[Rows][Cols];         ///< Text as of the last Render()
        int row = 0;                    ///< Cursor line
        int col = 0;                    ///< Cursor column (Cols = line full, wraps on the next character)
        bool newlinePending = false;    ///< A '\\n' arrived; move to the next line before the next character
        Escape escape = Escape::NONE;   ///< Escape sequence parsing state
};
