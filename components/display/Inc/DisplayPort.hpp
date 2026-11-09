#pragma once

/// @file DisplayPort.hpp
/// @brief Target specific display hooks used by Display.
/// @details These are the only functions that need to be implemented when porting the display to a new target or
///          panel. See DisplayPort.cpp for the ESP-WROVER-KIT implementation (ILI9341 or ST7789V over SPI).

#include <cstdint>

/// @brief Target specific display hooks
/// @details Static class: there is only one display. Any state the implementation needs (bus handles, pixel buffers)
///          is kept private in DisplayPort.cpp, since it depends on the target.
class DisplayPort
{
    public:

        DisplayPort() = delete;

        /// @brief Initialize the bus and the panel, leaving the backlight off
        /// @details May block for a few hundred milliseconds (panel reset and power up).
        static void Initialize();

        /// @brief Turn on the backlight
        static void BacklightOn();

        /// @brief Get the width of the panel
        /// @return width, in pixels
        static int GetWidth();

        /// @brief Get the height of the panel
        /// @return height, in pixels
        static int GetHeight();

        /// @brief Fill a rectangle with one color
        /// @details The rectangle must be inside the panel. Blocks until the pixels are sent.
        /// @param[in] x - left edge, in pixels
        /// @param[in] y - top edge, in pixels
        /// @param[in] w - width, in pixels (> 0)
        /// @param[in] h - height, in pixels (> 0)
        /// @param[in] color - RGB565 color
        static void FillRect(int x, int y, int w, int h, uint16_t color);

        /// @brief Draw a rectangle of pixels
        /// @details The rectangle must be inside the panel. Blocks until the pixels are sent, so the caller can reuse
        ///          the buffer as soon as this returns.
        /// @param[in] x - left edge, in pixels
        /// @param[in] y - top edge, in pixels
        /// @param[in] w - width, in pixels (> 0)
        /// @param[in] h - height, in pixels (> 0)
        /// @param[in] pixels - w * h RGB565 pixels, row by row, left to right
        static void DrawBitmap(int x, int y, int w, int h, const uint16_t* pixels);
};
