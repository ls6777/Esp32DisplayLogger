/// @file DisplayPort.cpp
/// @brief DisplayPort implementation for the ESP-WROVER-KIT 320x240 LCD (ESP-IDF SPI master driver).
/// @details Depending on its version, the kit has an ILI9341 (v4.1 and DevKitJ v1) or an ST7789V (v2 and v3)
///          controller. Both use the same pins and commands for drawing. They only differ in their initialization
///          sequence, so the controller is detected by reading its ID at start up. Based on the ESP-IDF
///          peripherals/spi_master/lcd example.
///
///          The panel is used in landscape orientation (320 x 240). Pixels are sent as big-endian RGB565, so they're
///          byte swapped into a DMA capable buffer before sending.

#include "DisplayPort.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr spi_host_device_t LCD_HOST = SPI2_HOST;   ///< SPI peripheral connected to the LCD

    constexpr gpio_num_t PIN_MISO = GPIO_NUM_25;        ///< SPI MISO (used to read the controller ID)
    constexpr gpio_num_t PIN_MOSI = GPIO_NUM_23;        ///< SPI MOSI
    constexpr gpio_num_t PIN_CLK = GPIO_NUM_19;         ///< SPI clock
    constexpr gpio_num_t PIN_CS = GPIO_NUM_22;          ///< SPI chip select
    constexpr gpio_num_t PIN_DC = GPIO_NUM_21;          ///< Data / command select (low = command)
    constexpr gpio_num_t PIN_RST = GPIO_NUM_18;         ///< Controller reset (active low)
    constexpr gpio_num_t PIN_BCKL = GPIO_NUM_5;         ///< Backlight control
    constexpr int BACKLIGHT_ON_LEVEL = 0;               ///< The kit's backlight is on when the pin is low

    constexpr int SPI_CLOCK_HZ = 10 * 1000 * 1000;      ///< 10 MHz, the max allowed by both controllers

    constexpr int WIDTH = 320;                          ///< Panel width (landscape), in pixels
    constexpr int HEIGHT = 240;                         ///< Panel height (landscape), in pixels

    constexpr int BUF_PIXELS = WIDTH * 8;               ///< Pixels sent per SPI transfer (8 full rows)

    constexpr uint8_t CMD_READ_ID = 0x04;               ///< Read display identification
    constexpr uint8_t CMD_COLUMN_SET = 0x2A;            ///< Column address set
    constexpr uint8_t CMD_PAGE_SET = 0x2B;              ///< Page (row) address set
    constexpr uint8_t CMD_MEMORY_WRITE = 0x2C;          ///< Memory write (pixel data follows)

    constexpr uint8_t DELAY_AFTER = 0x80;               ///< Init command flag: wait 100 ms after the command
    constexpr uint8_t END_OF_CMDS = 0xFF;               ///< Init command list terminator

    /// @brief One command of a controller initialization sequence
    struct InitCmd
    {
        uint8_t cmd;            ///< Command byte
        uint8_t data[16];       ///< Parameter bytes
        uint8_t dataBytes;      ///< Number of parameter bytes, ORed with DELAY_AFTER, or END_OF_CMDS
    };

    /// @brief ST7789V initialization: landscape, 16 bits per pixel
    DRAM_ATTR const InitCmd ST7789V_INIT[] =
    {
        {0x36, {(1 << 5) | (1 << 6)}, 1},   // Memory data access control: MX = MV = 1 (landscape)
        {0x3A, {0x55}, 1},                  // Interface pixel format: 16 bits per pixel
        {0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5},  // Porch setting
        {0xB7, {0x45}, 1},                  // Gate control
        {0xBB, {0x2B}, 1},                  // VCOM setting
        {0xC0, {0x2C}, 1},                  // LCM control
        {0xC2, {0x01, 0xFF}, 2},            // VDV and VRH command enable
        {0xC3, {0x11}, 1},                  // VRH set
        {0xC4, {0x20}, 1},                  // VDV set
        {0xC6, {0x0F}, 1},                  // Frame rate control: 60 Hz
        {0xD0, {0xA4, 0xA1}, 2},            // Power control 1
        {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14}, // Gamma +
        {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14}, // Gamma -
        {0x11, {0}, DELAY_AFTER},           // Sleep out
        {0x29, {0}, DELAY_AFTER},           // Display on
        {0, {0}, END_OF_CMDS}
    };

    /// @brief ILI9341 initialization: landscape, 16 bits per pixel
    DRAM_ATTR const InitCmd ILI9341_INIT[] =
    {
        {0xCF, {0x00, 0x83, 0x30}, 3},      // Power control B
        {0xED, {0x64, 0x03, 0x12, 0x81}, 4},// Power on sequence control
        {0xE8, {0x85, 0x01, 0x79}, 3},      // Driver timing control A
        {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},  // Power control A
        {0xF7, {0x20}, 1},                  // Pump ratio control
        {0xEA, {0x00, 0x00}, 2},            // Driver timing control B
        {0xC0, {0x26}, 1},                  // Power control 1
        {0xC1, {0x11}, 1},                  // Power control 2
        {0xC5, {0x35, 0x3E}, 2},            // VCOM control 1
        {0xC7, {0xBE}, 1},                  // VCOM control 2
        {0x36, {0x28}, 1},                  // Memory access control: MV = 1 (landscape), BGR = 1
        {0x3A, {0x55}, 1},                  // Pixel format: 16 bits per pixel
        {0xB1, {0x00, 0x1B}, 2},            // Frame rate control: 70 Hz
        {0xF2, {0x08}, 1},                  // 3 gamma control disabled
        {0x26, {0x01}, 1},                  // Gamma curve 1
        {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15}, // Gamma +
        {0xE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15}, // Gamma -
        {0xB7, {0x07}, 1},                  // Entry mode set
        {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},// Display function control
        {0x11, {0}, DELAY_AFTER},           // Sleep out
        {0x29, {0}, DELAY_AFTER},           // Display on
        {0, {0}, END_OF_CMDS}
    };

    spi_device_handle_t spi = nullptr;      ///< The LCD on the SPI bus

    /// @brief Pixels being sent, already byte swapped (must be DMA capable)
    DMA_ATTR uint16_t pixelBuf[BUF_PIXELS];

    //------------------------------------------------------------------
    // PreTransferCallback
    //------------------------------------------------------------------
    /// @brief Called by the SPI driver (in interrupt context) just before each transfer, to set the D/C line
    /// @param[in] t - transfer about to start. Its user field holds the D/C level (0 = command, 1 = data).
    void IRAM_ATTR PreTransferCallback(spi_transaction_t* t)
    {
        gpio_set_level(PIN_DC, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t->user)));
    }

    //------------------------------------------------------------------
    // Transmit
    //------------------------------------------------------------------
    /// @brief Send a transfer and wait for it to finish
    /// @param[in] t - transfer to send
    void Transmit(spi_transaction_t& t)
    {
        const esp_err_t err = spi_device_polling_transmit(spi, &t);
        assert(ESP_OK == err);
        (void)err;
    }

    //------------------------------------------------------------------
    // SendCmd
    //------------------------------------------------------------------
    /// @brief Send a command byte
    /// @param[in] cmd - command to send
    /// @param[in] keepCsActive - true to keep chip select active after the command (the bus must be acquired)
    void SendCmd(uint8_t cmd, bool keepCsActive = false)
    {
        spi_transaction_t t = {};
        t.length = 8;
        t.flags = SPI_TRANS_USE_TXDATA | (keepCsActive ? SPI_TRANS_CS_KEEP_ACTIVE : 0);
        t.tx_data[0] = cmd;
        t.user = reinterpret_cast<void*>(0);
        Transmit(t);
    }

    //------------------------------------------------------------------
    // SendData
    //------------------------------------------------------------------
    /// @brief Send parameter or pixel data
    /// @param[in] data - data to send (must be DMA capable if longer than 4 bytes)
    /// @param[in] len - length of data, in bytes
    void SendData(const void* data, size_t len)
    {
        if (len == 0)
        {
            return;
        }

        spi_transaction_t t = {};
        t.length = len * 8;
        t.tx_buffer = data;
        t.user = reinterpret_cast<void*>(1);
        Transmit(t);
    }

    //------------------------------------------------------------------
    // ReadId
    //------------------------------------------------------------------
    /// @brief Read the controller's 24-bit ID
    /// @return the ID: 0 for the ILI9341, non zero for the ST7789V
    uint32_t ReadId()
    {
        // Chip select must stay active between the command and the read, which needs the bus acquired
        ESP_ERROR_CHECK(spi_device_acquire_bus(spi, portMAX_DELAY));
        SendCmd(CMD_READ_ID, true);

        spi_transaction_t t = {};
        t.length = 8 * 3;
        t.flags = SPI_TRANS_USE_RXDATA;
        t.user = reinterpret_cast<void*>(1);
        Transmit(t);

        spi_device_release_bus(spi);

        uint32_t id = 0;
        std::memcpy(&id, t.rx_data, sizeof(id));
        return id;
    }

    //------------------------------------------------------------------
    // SetWindow
    //------------------------------------------------------------------
    /// @brief Set the rectangle that the next memory write fills, and start the memory write
    /// @param[in] x - left edge, in pixels
    /// @param[in] y - top edge, in pixels
    /// @param[in] w - width, in pixels
    /// @param[in] h - height, in pixels
    void SetWindow(int x, int y, int w, int h)
    {
        assert((x >= 0) && (y >= 0) && (w > 0) && (h > 0) && ((x + w) <= WIDTH) && ((y + h) <= HEIGHT));

        const int x2 = x + w - 1;
        const int y2 = y + h - 1;

        const uint8_t columns[4] = {static_cast<uint8_t>(x >> 8), static_cast<uint8_t>(x),
                                    static_cast<uint8_t>(x2 >> 8), static_cast<uint8_t>(x2)};
        const uint8_t pages[4] = {static_cast<uint8_t>(y >> 8), static_cast<uint8_t>(y),
                                  static_cast<uint8_t>(y2 >> 8), static_cast<uint8_t>(y2)};

        SendCmd(CMD_COLUMN_SET);
        SendData(columns, sizeof(columns));
        SendCmd(CMD_PAGE_SET);
        SendData(pages, sizeof(pages));
        SendCmd(CMD_MEMORY_WRITE);
    }

    //------------------------------------------------------------------
    // Swap
    //------------------------------------------------------------------
    /// @brief Convert an RGB565 pixel to the panel's byte order (big-endian)
    /// @param[in] color - RGB565 color
    /// @return color with its bytes swapped
    constexpr uint16_t Swap(uint16_t color)
    {
        return static_cast<uint16_t>((color << 8) | (color >> 8));
    }
}

//------------------------------------------------------------------
// Initialize
//------------------------------------------------------------------
void DisplayPort::Initialize()
{
    spi_bus_config_t busCfg = {};
    busCfg.mosi_io_num = PIN_MOSI;
    busCfg.miso_io_num = PIN_MISO;
    busCfg.sclk_io_num = PIN_CLK;
    busCfg.quadwp_io_num = -1;
    busCfg.quadhd_io_num = -1;
    busCfg.max_transfer_sz = sizeof(pixelBuf);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &busCfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devCfg = {};
    devCfg.clock_speed_hz = SPI_CLOCK_HZ;
    devCfg.mode = 0;
    devCfg.spics_io_num = PIN_CS;
    devCfg.queue_size = 1;
    devCfg.pre_cb = PreTransferCallback;
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devCfg, &spi));

    gpio_config_t ioCfg = {};
    ioCfg.pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST) | (1ULL << PIN_BCKL);
    ioCfg.mode = GPIO_MODE_OUTPUT;
    ioCfg.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&ioCfg));

    // Backlight off until the screen has been cleared
    gpio_set_level(PIN_BCKL, !BACKLIGHT_ON_LEVEL);

    // Reset the controller
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    const uint32_t id = ReadId();
    const InitCmd* cmds = (id == 0) ? ILI9341_INIT : ST7789V_INIT;
    printf("LCD ID: 0x%06lx, %s detected\r\n", (unsigned long)id, (id == 0) ? "ILI9341" : "ST7789V");

    for (; cmds->dataBytes != END_OF_CMDS; cmds++)
    {
        SendCmd(cmds->cmd);
        SendData(cmds->data, cmds->dataBytes & 0x1F);
        if (cmds->dataBytes & DELAY_AFTER)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

//------------------------------------------------------------------
// BacklightOn
//------------------------------------------------------------------
void DisplayPort::BacklightOn()
{
    gpio_set_level(PIN_BCKL, BACKLIGHT_ON_LEVEL);
}

//------------------------------------------------------------------
// GetWidth
//------------------------------------------------------------------
int DisplayPort::GetWidth()
{
    return WIDTH;
}

//------------------------------------------------------------------
// GetHeight
//------------------------------------------------------------------
int DisplayPort::GetHeight()
{
    return HEIGHT;
}

//------------------------------------------------------------------
// FillRect
//------------------------------------------------------------------
void DisplayPort::FillRect(int x, int y, int w, int h, uint16_t color)
{
    SetWindow(x, y, w, h);

    int remaining = w * h;
    const int fillLen = (remaining < BUF_PIXELS) ? remaining : BUF_PIXELS;
    const uint16_t swapped = Swap(color);
    for (int i = 0; i < fillLen; i++)
    {
        pixelBuf[i] = swapped;
    }

    while (remaining > 0)
    {
        const int len = (remaining < BUF_PIXELS) ? remaining : BUF_PIXELS;
        SendData(pixelBuf, static_cast<size_t>(len) * sizeof(uint16_t));
        remaining -= len;
    }
}

//------------------------------------------------------------------
// DrawBitmap
//------------------------------------------------------------------
void DisplayPort::DrawBitmap(int x, int y, int w, int h, const uint16_t* pixels)
{
    assert(nullptr != pixels);

    SetWindow(x, y, w, h);

    int remaining = w * h;
    while (remaining > 0)
    {
        const int len = (remaining < BUF_PIXELS) ? remaining : BUF_PIXELS;
        for (int i = 0; i < len; i++)
        {
            pixelBuf[i] = Swap(pixels[i]);
        }
        SendData(pixelBuf, static_cast<size_t>(len) * sizeof(uint16_t));
        pixels += len;
        remaining -= len;
    }
}
