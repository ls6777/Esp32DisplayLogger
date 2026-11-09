#pragma once

/// @file DisplayTask.hpp
/// @brief Task that owns the LCD: a terminal showing the UART output, and the CPU usage.

#include "CpuUsage.hpp"
#include "Message.hpp"
#include "TargetPort.hpp"

/// @brief Display task
/// @details The only task that draws on the LCD (Display isn't thread safe). Green text on black, like an old terminal.
///
///          The board is viewed with the panel turned 90 degrees (the panel's x axis runs up), so all text is drawn
///          rotated clockwise, which makes it upright as viewed. As viewed:
///          - left: a terminal of CONSOLE_COLS x CONSOLE_ROWS characters showing the same output as the UART. Lines
///            fill from the top; long lines wrap; once full, each new line goes on the bottom and the rest scroll up.
///          - top right corner: the CPU usage ("CPU 25%"), updated every 500 ms
///
///          The terminal text arrives through MirrorOutput(), which PrintUtils calls (from the PrintfTask) with
///          everything it sends to the UART (see PrintUtils::SetMirror()). It is buffered and drawn every 50 ms. Only
///          the characters that changed are redrawn.
///
///          Registers with the task monitor with a 2 second timeout and checks in when the monitor asks.
class DisplayTask
{
    public:

        static constexpr int CONSOLE_COLS = 32;     ///< Terminal width, in characters (as viewed)
        static constexpr int CONSOLE_ROWS = 40;     ///< Terminal height, in lines (as viewed)

        /// @brief Create this task's message queue and post the initialize message to it
        /// @note Must be called before Run(). Display::Initialize() must have been called.
        void Initialize();

        /// @brief Execution of DisplayTask
        /// @details Main loop: processes messages posted to the queue, draws new terminal text at least every 50 ms,
        ///          and updates the CPU usage every 500 ms. The CPU updates keep to the 500 ms schedule however many
        ///          messages arrive. Never returns.
        void Run();

        /// @brief Request a checkin from this task (task monitor checkin callback)
        /// @details Posts a checkin message to this task's queue. The task then checks in with the task monitor.
        static void TaskCheckin();

        /// @brief Post a message to shut down this task
        void Shutdown();

        /// @brief Add output to the terminal (PrintUtils mirror function, see PrintUtils::SetMirror())
        /// @details Copies the data into the terminal buffer, to be drawn by this task. Quick and non-blocking. If the
        ///          buffer is full, the rest of the data is dropped from the terminal (the UART is not affected).
        /// @note Only one task may call this (the PrintfTask).
        /// @param[in] data - output to add
        /// @param[in] len - length of the output, in bytes
        static void MirrorOutput(const char* data, int len);

    private:

        /// @brief Post a message to this task's queue
        /// @param[in] id - ID of the message to send
        static void SendMessage(Message::MsgId id);

        /// @brief Handle the initialize message: register with the task monitor, start measuring the CPU usage and
        ///        show a placeholder for it until the first measurement
        void HandleInitialize();

        /// @brief Move new output from the terminal buffer into the terminal, and draw the characters that changed
        void RefreshConsole();

        /// @brief Measure the CPU usage since the last update and draw it
        void UpdateCpuUsage();

        /// @brief Draw the CPU usage text in the top right corner (as viewed)
        /// @param[in] text - text to draw; always CPU_TEXT_LENGTH characters, so it covers the previous text
        void DrawCpuText(const char* text);

        /// @brief Handle the shutdown message: print a message and stop processing
        void HandleShutdown();

        CpuUsage cpuUsage;          ///< CPU usage calculation

        static QHandle msgQHandle;  ///< This task's message queue
};
