/// @file DisplayTask.cpp
/// @brief Display task implementation (see DisplayTask.hpp).

#include "DisplayTask.hpp"
#include "Display.hpp"
#include "PrintBuffer.hpp"
#include "TaskMonitor.hpp"
#include "TextConsole.hpp"
#include "Utils.hpp"

#include <cstdio>

namespace
{
    constexpr uint32_t MSG_Q_LENGTH = 10;                   ///< Number of messages the queue can hold
    constexpr uint32_t CPU_UPDATE_PERIOD = 500;             ///< Period (ms) for updating the CPU usage
    constexpr uint32_t CONSOLE_PERIOD = 50;                 ///< Longest time (ms) before new terminal text is drawn
    constexpr uint32_t CHECKIN_TIMEOUT = 2 * MS_PER_SEC;    ///< Task monitor timeout (ms)

    constexpr const char* CPU_PLACEHOLDER = "CPU --%";      ///< Shown until the first CPU usage measurement
    constexpr int CPU_TEXT_LENGTH = 7;                      ///< Length of the CPU text, e.g. "CPU 25%" or "CPU100%"

    constexpr int TEXT_SCALE = 1;                           ///< Size of the text (the font's native 6x8 pixels)
    constexpr uint16_t TEXT_COLOR = Color::GREEN;           ///< Text color: green on black, like an old terminal
    constexpr uint16_t BACKGROUND_COLOR = Color::BLACK;     ///< Text background color

    /// @brief Orientation of the text: rotated clockwise on the panel, which is upright as the board is viewed
    constexpr TextRenderer::Rotation TEXT_ROTATION = TextRenderer::Rotation::CW90;

    /// @brief Size of one character on the panel (8 x 6 pixels, rotated)
    constexpr int CELL_WIDTH = TextRenderer::CellWidth(TEXT_SCALE, TEXT_ROTATION);
    constexpr int CELL_HEIGHT = TextRenderer::CellHeight(TEXT_SCALE, TEXT_ROTATION);

    /// @brief Size of the terminal buffer, in bytes: output waiting to be drawn (filled by the PrintfTask)
    constexpr int CONSOLE_BUFFER_SIZE = 2048;

    using Console = TextConsole<DisplayTask::CONSOLE_COLS, DisplayTask::CONSOLE_ROWS>;  ///< The terminal

    Console console;                                    ///< Terminal text (drawn by this task only)
    PrintBuffer<CONSOLE_BUFFER_SIZE> consoleBuffer;     ///< Output from the PrintfTask, waiting for the terminal

    //------------------------------------------------------------------
    // WriteToConsole
    //------------------------------------------------------------------
    /// @brief Output function for consoleBuffer.Transmit(): adds the output to the terminal
    /// @param[in] data - output
    /// @param[in] len - length of the output, in bytes
    /// @return len (all of it is taken)
    int WriteToConsole(const char* data, int len)
    {
        console.Write(data, len);
        return len;
    }

    //------------------------------------------------------------------
    // DrawConsoleChar
    //------------------------------------------------------------------
    /// @brief Draw one terminal character
    /// @details As viewed, terminal lines run top to bottom and characters left to right. On the panel (turned 90
    ///          degrees) that is: lines from the panel's right edge leftwards, characters from the panel's top down.
    /// @param[in] row - terminal line (0 = top as viewed)
    /// @param[in] col - terminal column (0 = left as viewed)
    /// @param[in] c - character to draw
    void DrawConsoleChar(int row, int col, char c)
    {
        const int x = Display::GetWidth() - ((row + 1) * CELL_WIDTH);
        const int y = col * CELL_HEIGHT;
        Display::DrawChar(x, y, c, TEXT_COLOR, BACKGROUND_COLOR, TEXT_SCALE, TEXT_ROTATION);
    }
}

QHandle DisplayTask::msgQHandle = nullptr;

//------------------------------------------------------------------
// SendMessage
//------------------------------------------------------------------
void DisplayTask::SendMessage(Message::MsgId id)
{
    const Message msg(id);
    const StatusType status = TargetPort::QSend(msgQHandle, &msg);
    ASSERT(PASS == status);
    (void)status;
}

//------------------------------------------------------------------
// TaskCheckin
//------------------------------------------------------------------
void DisplayTask::TaskCheckin()
{
    SendMessage(Message::TASK_CHECKIN);
}

//------------------------------------------------------------------
// Shutdown
//------------------------------------------------------------------
void DisplayTask::Shutdown()
{
    SendMessage(Message::SHUTDOWN);
}

//------------------------------------------------------------------
// MirrorOutput
//------------------------------------------------------------------
void DisplayTask::MirrorOutput(const char* data, int len)
{
    consoleBuffer.Write(data, len);
}

//------------------------------------------------------------------
// Initialize
//------------------------------------------------------------------
void DisplayTask::Initialize()
{
    msgQHandle = TargetPort::CreateQ(MSG_Q_LENGTH, sizeof(Message));
    ASSERT(nullptr != msgQHandle);
    SendMessage(Message::INITIALIZE);
}

//------------------------------------------------------------------
// Run
//------------------------------------------------------------------
void DisplayTask::Run()
{
    Message msg;
    uint32_t nextCpuUpdate = TargetPort::GetTimeMs() + CPU_UPDATE_PERIOD;

    while (true)
    {
        // Wait for a message, but no longer than the terminal period, and only until the next CPU update is due
        const int32_t untilCpuUpdate = static_cast<int32_t>(nextCpuUpdate - TargetPort::GetTimeMs());
        uint32_t wait = (untilCpuUpdate > 0) ? static_cast<uint32_t>(untilCpuUpdate) : 0;
        if (wait > CONSOLE_PERIOD)
        {
            wait = CONSOLE_PERIOD;
        }

        if (TRUE == TargetPort::QRecv(msgQHandle, &msg, MS_TO_TICKS(wait)))
        {
            switch (msg.msgId)
            {
                case Message::TASK_CHECKIN:
                    TaskMonitor::TaskCheckin();
                    break;

                case Message::INITIALIZE:
                    HandleInitialize();
                    break;

                case Message::SHUTDOWN:
                    HandleShutdown();
                    break;

                default:
                    printf("DisplayTask - unknown msgId: %d\r\n", msg.msgId);
                    break;
            }
        }

        RefreshConsole();

        if (static_cast<int32_t>(TargetPort::GetTimeMs() - nextCpuUpdate) >= 0)
        {
            UpdateCpuUsage();

            // Keep to the schedule. If an update was missed entirely, restart the schedule instead of catching up.
            nextCpuUpdate += CPU_UPDATE_PERIOD;
            if (static_cast<int32_t>(nextCpuUpdate - TargetPort::GetTimeMs()) <= 0)
            {
                nextCpuUpdate = TargetPort::GetTimeMs() + CPU_UPDATE_PERIOD;
            }
        }
    }
}

//------------------------------------------------------------------
// HandleInitialize
//------------------------------------------------------------------
void DisplayTask::HandleInitialize()
{
    // The terminal and the CPU text must fit side by side (as viewed), with a one character gap between them
    ASSERT((CONSOLE_ROWS * CELL_WIDTH) <= Display::GetWidth());
    ASSERT(((CONSOLE_COLS + 1 + CPU_TEXT_LENGTH) * CELL_HEIGHT) <= Display::GetHeight());

    // Register this task with the Task Monitor
    TaskMonitor::Register(CHECKIN_TIMEOUT, &TaskCheckin);

    // Start measuring: the first reading is the starting point for the first update
    uint32_t idleTime = 0;
    uint32_t totalTime = 0;
    TargetPort::GetCpuTimes(idleTime, totalTime);
    cpuUsage.Update(idleTime, totalTime);
    DrawCpuText(CPU_PLACEHOLDER);

    printf("DisplayTask Initialized\r\n");
}

//------------------------------------------------------------------
// RefreshConsole
//------------------------------------------------------------------
void DisplayTask::RefreshConsole()
{
    consoleBuffer.Transmit(WriteToConsole);
    console.Render(DrawConsoleChar);
}

//------------------------------------------------------------------
// UpdateCpuUsage
//------------------------------------------------------------------
void DisplayTask::UpdateCpuUsage()
{
    uint32_t idleTime = 0;
    uint32_t totalTime = 0;
    TargetPort::GetCpuTimes(idleTime, totalTime);
    const uint32_t percent = cpuUsage.Update(idleTime, totalTime);

    if (cpuUsage.HasValue())
    {
        char text[CPU_TEXT_LENGTH + 1];
        snprintf(text, sizeof(text), "CPU%3lu%%", (unsigned long)percent);
        DrawCpuText(text);
    }
}

//------------------------------------------------------------------
// DrawCpuText
//------------------------------------------------------------------
void DisplayTask::DrawCpuText(const char* text)
{
    // Top right corner as viewed: the panel's bottom right, so the text ends at the panel's bottom edge
    Display::DrawString(Display::GetWidth() - CELL_WIDTH, Display::GetHeight() - (CPU_TEXT_LENGTH * CELL_HEIGHT), text,
                        TEXT_COLOR, BACKGROUND_COLOR, TEXT_SCALE, TEXT_ROTATION);
}

//------------------------------------------------------------------
// HandleShutdown
//------------------------------------------------------------------
void DisplayTask::HandleShutdown()
{
    printf("DisplayTask Shutdown\r\n");
    // wait forever
    DELAY(MAX_DELAY);
}
