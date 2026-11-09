/// @file test_DisplayTask.cpp
/// @brief Host unit tests for DisplayTask.cpp.
/// @details Uses the real TaskMonitor, Display, TextRenderer, font, TextConsole, PrintBuffer and CpuUsage. Only the
///          port layers are fakes: the OS (fakes/TargetPort.hpp, which also simulates the CPU load) and DisplayPort
///          (defined here).
///
///          The terminal text lives in DisplayTask.cpp for the whole program, so the terminal tests depend on their
///          order: each one continues from the lines the previous ones wrote. No other test writes to the terminal.

#include "DisplayTask.hpp"

#include "Display.hpp"
#include "DisplayPort.hpp"
#include "StdoutCapture.hpp"
#include "TargetPort.hpp"
#include "TaskMonitor.hpp"
#include "TextRenderer.hpp"

#include "unity.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr int WIDTH = 320;      ///< Width of the fake panel
    constexpr int HEIGHT = 240;     ///< Height of the fake panel

    constexpr int SCALE = 1;                                            ///< Text size used by DisplayTask
    constexpr TextRenderer::Rotation ROTATION = TextRenderer::Rotation::CW90;  ///< Text orientation used by DisplayTask
    constexpr int CELL_W = TextRenderer::CellWidth(SCALE, ROTATION);    ///< Character width on the panel (8)
    constexpr int CELL_H = TextRenderer::CellHeight(SCALE, ROTATION);   ///< Character height on the panel (6)

    constexpr int CPU_X = WIDTH - CELL_W;           ///< CPU text runs down the panel's right edge (top as viewed)
    constexpr int CPU_Y = HEIGHT - (7 * CELL_H);    ///< The 7 character CPU text ends at the panel's bottom (right)

    constexpr int CONSOLE_END_Y = DisplayTask::CONSOLE_COLS * CELL_H;  ///< Terminal lines end here on the panel (192)

    /// @brief One DisplayPort::DrawBitmap() call
    struct Bitmap
    {
        int x;                          ///< Left edge
        int y;                          ///< Top edge
        int w;                          ///< Width
        int h;                          ///< Height
        std::vector<uint16_t> pixels;   ///< Copy of the pixels
        uint32_t timeMs;                ///< Fake time when it was drawn
    };

    std::vector<Bitmap> bitmaps;    ///< Every DrawBitmap() call, in order

    FakeTask displayTask{"DisplayTask"};    ///< The task running DisplayTask

    QHandle monitorQ = nullptr;     ///< The task monitor's queue
    QHandle displayQ = nullptr;     ///< DisplayTask's queue

    std::unique_ptr<DisplayTask> task;  ///< Task under test (kept across runs within a test, as it holds the usage)

    /// @brief Run the task
    /// @return everything printed to stdout
    std::string RunTask()
    {
        FakeTargetPort::currentTask = &displayTask;
        StdoutCapture capture;
        capture.Start();
        try
        {
            task->Run();
        }
        catch (const StopRun&)
        {
        }
        return capture.Stop();
    }

    /// @brief Run the task for a number of queue reads (each read gets a message, or waits up to 50 ms)
    /// @param[in] reads - number of queue reads
    /// @return everything printed to stdout
    std::string RunReads(int reads)
    {
        FakeTargetPort::recvBudget = reads;
        return RunTask();
    }

    /// @brief Run the task until the fake clock has advanced by a time
    /// @details The task is restarted, so its 500 ms CPU schedule starts now.
    /// @param[in] ms - time to run for
    /// @param[in] onRecv - optional, called at every queue read (e.g. to make a message arrive during a wait)
    /// @return everything printed to stdout
    std::string RunForMs(uint32_t ms, std::function<void()> onRecv = nullptr)
    {
        const uint32_t end = FakeTargetPort::timeMs + ms;
        FakeTargetPort::recvBudget = -1;
        FakeTargetPort::onRecv = [end, onRecv]()
        {
            if (onRecv)
            {
                onRecv();
            }
            if (FakeTargetPort::timeMs >= end)
            {
                throw StopRun();
            }
        };
        const std::string printed = RunTask();
        FakeTargetPort::onRecv = nullptr;
        return printed;
    }

    /// @brief Add text to the terminal, as the PrintfTask would
    /// @param[in] text - text to add
    void Mirror(const char* text)
    {
        DisplayTask::MirrorOutput(text, static_cast<int>(std::strlen(text)));
    }

    /// @brief Get the messages sent to the task monitor
    /// @return messages, oldest first
    std::vector<TaskMonitor::TaskMonitorMessage> MonitorMessages()
    {
        std::vector<TaskMonitor::TaskMonitorMessage> msgs;
        for (const FakeTargetPort::SentItem& item : FakeTargetPort::sent)
        {
            if (item.queue == monitorQ)
            {
                msgs.push_back(item.As<TaskMonitor::TaskMonitorMessage>());
            }
        }
        return msgs;
    }

    /// @brief Get the drawn characters of the CPU text (panel bottom right)
    /// @return the bitmaps, in order
    std::vector<Bitmap> CpuBitmaps()
    {
        std::vector<Bitmap> result;
        for (const Bitmap& b : bitmaps)
        {
            if (b.y >= CPU_Y)
            {
                result.push_back(b);
            }
        }
        return result;
    }

    /// @brief Get the drawn characters of the terminal
    /// @return the bitmaps, in order
    std::vector<Bitmap> ConsoleBitmaps()
    {
        std::vector<Bitmap> result;
        for (const Bitmap& b : bitmaps)
        {
            if (b.y < CONSOLE_END_Y)
            {
                result.push_back(b);
            }
        }
        return result;
    }

    /// @brief Check that a bitmap is one character as DisplayTask draws it
    /// @param[in] b - bitmap to check
    /// @param[in] c - expected character
    /// @param[in] x - expected left edge
    /// @param[in] y - expected top edge
    void CheckChar(const Bitmap& b, char c, int x, int y)
    {
        char msg[48];
        std::snprintf(msg, sizeof(msg), "'%c' at (%d, %d)", c, x, y);
        TEST_ASSERT_EQUAL_MESSAGE(x, b.x, msg);
        TEST_ASSERT_EQUAL_MESSAGE(y, b.y, msg);
        TEST_ASSERT_EQUAL_MESSAGE(CELL_W, b.w, msg);
        TEST_ASSERT_EQUAL_MESSAGE(CELL_H, b.h, msg);

        std::vector<uint16_t> expected(TextRenderer::CellPixels(SCALE));
        TextRenderer::RenderChar(c, Color::GREEN, Color::BLACK, SCALE, expected.data(), ROTATION);
        TEST_ASSERT_EQUAL_HEX16_ARRAY_MESSAGE(expected.data(), b.pixels.data(), expected.size(), msg);
    }

    /// @brief Check that some bitmaps are the CPU text
    /// @param[in] drawn - bitmaps
    /// @param[in] first - index of the first character
    /// @param[in] text - expected 7 character text
    void CheckCpuText(const std::vector<Bitmap>& drawn, size_t first, const char* text)
    {
        TEST_ASSERT_TRUE_MESSAGE(drawn.size() >= first + 7, "CPU text not drawn");
        for (int i = 0; i < 7; i++)
        {
            CheckChar(drawn[first + i], text[i], CPU_X, CPU_Y + (i * CELL_H));
        }
    }

    /// @brief Panel position of a terminal line: lines run from the panel's right edge leftwards
    /// @param[in] row - terminal line (0 = top as viewed)
    /// @return left edge on the panel
    int RowX(int row)
    {
        return WIDTH - ((row + 1) * CELL_W);
    }

    /// @brief Panel position of a terminal column: characters run from the panel's top down
    /// @param[in] col - terminal column (0 = left as viewed)
    /// @return top edge on the panel
    int ColY(int col)
    {
        return col * CELL_H;
    }

    /// @brief Run the INITIALIZE message (draws the CPU placeholder)
    void RunInitialize()
    {
        RunReads(1);
        TEST_ASSERT_EQUAL(7, bitmaps.size());
    }
}

//------------------------------------------------------------------
// Fake DisplayPort
//------------------------------------------------------------------
void DisplayPort::Initialize() {}

void DisplayPort::BacklightOn() {}

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
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
}

void DisplayPort::DrawBitmap(int x, int y, int w, int h, const uint16_t* pixels)
{
    bitmaps.push_back(Bitmap{x, y, w, h, std::vector<uint16_t>(pixels, pixels + (w * h)), FakeTargetPort::timeMs});
}

/// @brief Unity: called before each test. Creates the task monitor's queue, then DisplayTask's (holding INITIALIZE).
void setUp(void)
{
    FakeTargetPort::Reset();
    bitmaps.clear();

    TaskMonitor::Initialize();
    monitorQ = FakeTargetPort::sent.back().queue;

    FakeTargetPort::currentTask = &displayTask;
    task = std::make_unique<DisplayTask>();
    task->Initialize();
    displayQ = FakeTargetPort::sent.back().queue;
}

/// @brief Unity: called after each test
void tearDown(void)
{
    task.reset();
}

//------------------------------------------------------------------
// Tests: task
//------------------------------------------------------------------

/// @brief Initialize creates the task's own queue and posts INITIALIZE to it
void test_Initialize_PostsInitialize(void)
{
    TEST_ASSERT_NOT_EQUAL(monitorQ, displayQ);
    TEST_ASSERT_EQUAL(1, FakeTargetPort::QueueCount(displayQ));
    TEST_ASSERT_EQUAL(Message::INITIALIZE, FakeTargetPort::sent.back().As<Message>().msgId);
}

/// @brief On INITIALIZE the task registers with the monitor: as itself, 2 s timeout, its TaskCheckin callback
void test_HandleInitialize_RegistersWithMonitor(void)
{
    const std::string printed = RunReads(1);

    const std::vector<TaskMonitor::TaskMonitorMessage> msgs = MonitorMessages();
    TEST_ASSERT_EQUAL(2, msgs.size());     // the monitor's own INITIALIZE, then REGISTER
    TEST_ASSERT_EQUAL(Message::REGISTER, msgs[1].msgId);
    TEST_ASSERT_EQUAL_PTR(&displayTask, msgs[1].taskId);
    TEST_ASSERT_EQUAL_UINT32(2000, msgs[1].maxResponseTime);
    TEST_ASSERT_EQUAL_PTR(&DisplayTask::TaskCheckin, msgs[1].checkinCall);
    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "DisplayTask Initialized"));
}

/// @brief On INITIALIZE only the CPU placeholder is drawn: top right as viewed (panel x 312, y 198 down to the
///        bottom edge), green on black, rotated, scale 1
void test_HandleInitialize_DrawsCpuPlaceholder(void)
{
    RunInitialize();

    TEST_ASSERT_EQUAL(312, CPU_X);
    TEST_ASSERT_EQUAL(198, CPU_Y);
    CheckCpuText(bitmaps, 0, "CPU --%");
}

/// @brief The terminal and the CPU text sit side by side (as viewed) with a one character gap
void test_Layout_ConsoleBesideCpu(void)
{
    TEST_ASSERT_EQUAL(32, DisplayTask::CONSOLE_COLS);
    TEST_ASSERT_EQUAL(40, DisplayTask::CONSOLE_ROWS);
    TEST_ASSERT_EQUAL(192, CONSOLE_END_Y);
    TEST_ASSERT_EQUAL(CONSOLE_END_Y + CELL_H, CPU_Y);                     // one character gap
    TEST_ASSERT_EQUAL(0, RowX(DisplayTask::CONSOLE_ROWS - 1));            // the bottom line uses the panel's left edge
}

//------------------------------------------------------------------
// Tests: CPU usage
//------------------------------------------------------------------

/// @brief The CPU usage is redrawn every 500 ms with the measured load
void test_CpuUsage_UpdatedEvery500ms(void)
{
    RunInitialize();
    FakeTargetPort::cpuBusyPercent = 25;
    const uint32_t start = FakeTargetPort::timeMs;

    RunForMs(1500);

    const std::vector<Bitmap> cpu = CpuBitmaps();
    TEST_ASSERT_EQUAL(7 + (3 * 7), cpu.size());     // placeholder, then 3 updates
    for (int update = 0; update < 3; update++)
    {
        const size_t first = 7 + (update * 7);
        CheckCpuText(cpu, first, "CPU 25%");
        TEST_ASSERT_EQUAL_UINT32(start + (500 * (update + 1)), cpu[first].timeMs);
    }
}

/// @brief Each update shows the load since the previous update, including 0 % and 100 % (no space: "CPU100%")
void test_CpuUsage_FollowsLoad(void)
{
    RunInitialize();

    const uint32_t loads[] = {10, 80, 0, 100, 7};
    const char* texts[] = {"CPU 10%", "CPU 80%", "CPU  0%", "CPU100%", "CPU  7%"};
    for (size_t i = 0; i < 5; i++)
    {
        FakeTargetPort::cpuBusyPercent = loads[i];
        const size_t first = CpuBitmaps().size();
        RunForMs(500);  // one more update (the same task, so the usage continues from the last reading)
        CheckCpuText(CpuBitmaps(), first, texts[i]);
    }
}

/// @brief A checkin request is answered as the task, and doesn't push the next CPU update back
void test_CheckinRequest_DoesNotDelayUpdate(void)
{
    RunInitialize();
    const uint32_t start = FakeTargetPort::timeMs;

    // The checkin request arrives 200 ms into the wait for the first update
    bool sent = false;
    RunForMs(500, [&sent]()
    {
        if (!sent)
        {
            sent = true;
            FakeTargetPort::AdvanceTime(200);
            DisplayTask::TaskCheckin();
        }
    });

    const TaskMonitor::TaskMonitorMessage last = MonitorMessages().back();
    TEST_ASSERT_EQUAL(Message::TASK_CHECKIN, last.msgId);
    TEST_ASSERT_EQUAL_PTR(&displayTask, last.taskId);

    const std::vector<Bitmap> cpu = CpuBitmaps();
    TEST_ASSERT_EQUAL(7 + 7, cpu.size());
    TEST_ASSERT_EQUAL_UINT32(start + 500, cpu[7].timeMs);
}

/// @brief If the task was held up past an update, it updates once and restarts the schedule instead of catching up
void test_LateUpdate_DoesNotCatchUp(void)
{
    RunInitialize();
    const uint32_t start = FakeTargetPort::timeMs;

    // The task is held up for 1.7 s (more than 3 periods); a message then wakes it
    bool sent = false;
    RunForMs(2200, [&sent]()
    {
        if (!sent)
        {
            sent = true;
            FakeTargetPort::AdvanceTime(1700);
            DisplayTask::TaskCheckin();
        }
    });

    const std::vector<Bitmap> cpu = CpuBitmaps();
    TEST_ASSERT_EQUAL(7 + (2 * 7), cpu.size());         // two updates, not four
    TEST_ASSERT_EQUAL_UINT32(start + 1700, cpu[7].timeMs);   // straight away
    TEST_ASSERT_EQUAL_UINT32(start + 2200, cpu[14].timeMs);  // then 500 ms later
}

//------------------------------------------------------------------
// Tests: terminal (in order: each continues from the previous one)
//------------------------------------------------------------------

/// @brief Mirrored output appears on the first terminal line within 50 ms: top left as viewed (the panel's top right)
void test_Console_ShowsMirroredText(void)
{
    RunInitialize();
    const uint32_t start = FakeTargetPort::timeMs;

    Mirror("Hi\r\n");
    RunForMs(50);

    const std::vector<Bitmap> console = ConsoleBitmaps();
    TEST_ASSERT_EQUAL(2, console.size());
    CheckChar(console[0], 'H', RowX(0), ColY(0));
    CheckChar(console[1], 'i', RowX(0), ColY(1));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(start + 50, console[0].timeMs);
}

/// @brief The next line goes underneath (as viewed): one character further left on the panel
void test_Console_NextLineGoesBelow(void)
{
    RunInitialize();
    Mirror("ok\r\n");
    RunForMs(50);

    const std::vector<Bitmap> console = ConsoleBitmaps();
    TEST_ASSERT_EQUAL(2, console.size());
    CheckChar(console[0], 'o', RowX(1), ColY(0));
    CheckChar(console[1], 'k', RowX(1), ColY(1));
}

/// @brief Lines longer than the terminal wrap onto the next line
void test_Console_LongLineWraps(void)
{
    RunInitialize();
    Mirror("0123456789012345678901234567890123456789\r\n");     // 40 characters: 32, then 8 on the next line
    RunForMs(50);

    const std::vector<Bitmap> console = ConsoleBitmaps();
    TEST_ASSERT_EQUAL(40, console.size());
    CheckChar(console[0], '0', RowX(2), ColY(0));
    CheckChar(console[31], '1', RowX(2), ColY(31));
    CheckChar(console[32], '2', RowX(3), ColY(0));
    CheckChar(console[39], '9', RowX(3), ColY(7));
}

/// @brief Once full, the newest line is written on the bottom and the rest shift up; only changed characters are
///        redrawn
void test_Console_ScrollsWhenFull(void)
{
    RunInitialize();

    // Lines 0-3 are used by the previous tests; fill lines 4-39
    char line[16];
    for (int i = 4; i < DisplayTask::CONSOLE_ROWS; i++)
    {
        std::snprintf(line, sizeof(line), "Line %02d\r\n", i);
        Mirror(line);
    }
    RunForMs(50);
    TEST_ASSERT_EQUAL(7, CpuBitmaps().size());  // (just the placeholder)
    bitmaps.clear();

    Mirror("Line 40\r\n");
    RunForMs(50);

    // Bottom line: "Line 39" became "Line 40", so only the two digits are redrawn
    std::vector<Bitmap> bottom;
    std::vector<Bitmap> top;
    for (const Bitmap& b : ConsoleBitmaps())
    {
        if (b.x == RowX(DisplayTask::CONSOLE_ROWS - 1))
        {
            bottom.push_back(b);
        }
        if (b.x == RowX(0))
        {
            top.push_back(b);
        }
    }
    TEST_ASSERT_EQUAL(2, bottom.size());
    CheckChar(bottom[0], '4', RowX(DisplayTask::CONSOLE_ROWS - 1), ColY(5));
    CheckChar(bottom[1], '0', RowX(DisplayTask::CONSOLE_ROWS - 1), ColY(6));

    // Top line: "Hi" scrolled off, "ok" moved up
    TEST_ASSERT_EQUAL(2, top.size());
    CheckChar(top[0], 'o', RowX(0), ColY(0));
    CheckChar(top[1], 'k', RowX(0), ColY(1));

    // Far less than redrawing the whole terminal (32 x 40 = 1280 characters)
    TEST_ASSERT_LESS_THAN(200, ConsoleBitmaps().size());
}

//------------------------------------------------------------------
// Tests: other messages
//------------------------------------------------------------------

/// @brief Unknown messages are reported and ignored
void test_UnknownMessage_Reported(void)
{
    RunInitialize();
    const Message bad(Message::NUM_MSG_IDS);
    TEST_ASSERT_EQUAL(PASS, TargetPort::QSend(displayQ, &bad));

    const std::string printed = RunReads(1);
    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "DisplayTask - unknown msgId"));
}

/// @brief Shutdown prints a message and then blocks forever
void test_Shutdown_BlocksForever(void)
{
    RunInitialize();
    task->Shutdown();

    const std::string printed = RunReads(5);

    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "DisplayTask Shutdown"));
    TEST_ASSERT_EQUAL(1, FakeTargetPort::delays.size());
    TEST_ASSERT_EQUAL_UINT32(MAX_DELAY, FakeTargetPort::delays[0]);
}

/// @brief Run all tests (the terminal tests must stay in order, see the file description)
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Initialize_PostsInitialize);
    RUN_TEST(test_HandleInitialize_RegistersWithMonitor);
    RUN_TEST(test_HandleInitialize_DrawsCpuPlaceholder);
    RUN_TEST(test_Layout_ConsoleBesideCpu);
    RUN_TEST(test_CpuUsage_UpdatedEvery500ms);
    RUN_TEST(test_CpuUsage_FollowsLoad);
    RUN_TEST(test_CheckinRequest_DoesNotDelayUpdate);
    RUN_TEST(test_LateUpdate_DoesNotCatchUp);
    RUN_TEST(test_Console_ShowsMirroredText);
    RUN_TEST(test_Console_NextLineGoesBelow);
    RUN_TEST(test_Console_LongLineWraps);
    RUN_TEST(test_Console_ScrollsWhenFull);
    RUN_TEST(test_UnknownMessage_Reported);
    RUN_TEST(test_Shutdown_BlocksForever);
    return UNITY_END();
}
