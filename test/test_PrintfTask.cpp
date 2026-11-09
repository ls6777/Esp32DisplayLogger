/// @file test_PrintfTask.cpp
/// @brief Host unit tests for PrintfTask.cpp, with the real PrintUtils, a fake StdOutPort and the fake OS port.

#include "PrintfTask.hpp"
#include "PrintUtils.hpp"
#include "StdOutPort.hpp"
#include "StdoutCapture.hpp"
#include "TargetPort.hpp"

#include "unity.h"

#include <cstring>
#include <string>

namespace
{
    int redirectCalls = 0;      ///< Number of StdOutPort::Redirect() calls
    std::string output;         ///< Everything output through StdOutPort::Write()
    int writeResult = 0;        ///< If non-zero, StdOutPort::Write() returns this instead of writing (e.g. an error)

    /// @brief Put text in the printf buffer, as printf would after the redirect
    /// @param[in] text - text to write
    void Print(const char* text)
    {
        PrintUtils::Write(text, static_cast<int>(std::strlen(text)));
    }

    /// @brief Run the printf task until it has waited a number of times
    /// @param[in] loops - number of loops (each loop transmits, then waits)
    void RunLoops(int loops)
    {
        FakeTargetPort::delayBudget = loops - 1;
        PrintfTask task;
        try
        {
            task.Run();
        }
        catch (const StopRun&)
        {
        }
    }
}

//------------------------------------------------------------------
// Fake StdOutPort
//------------------------------------------------------------------
void StdOutPort::Redirect()
{
    redirectCalls++;
}

int StdOutPort::Write(const char* data, int len)
{
    if (writeResult != 0)
    {
        return writeResult;
    }
    output.append(data, static_cast<size_t>(len));
    return len;
}

/// @brief Unity: called before each test
void setUp(void)
{
    // PrintUtils owns a single buffer for the whole program, so empty it through the fake port
    writeResult = 0;
    PrintUtils::TransmitData();

    redirectCalls = 0;
    output.clear();
    FakeTargetPort::Reset();
}

/// @brief Unity: called after each test
void tearDown(void) {}

//------------------------------------------------------------------
// Tests
//------------------------------------------------------------------

/// @brief Initialize redirects stdout into the printf buffer
void test_Initialize_RedirectsStdOut(void)
{
    PrintfTask::Initialize();
    TEST_ASSERT_EQUAL(1, redirectCalls);
}

/// @brief Each loop sends everything in the buffer, then waits 10 ms
void test_Run_SendsBufferThenWaits10ms(void)
{
    Print("Hello ");
    Print("world");

    RunLoops(3);

    TEST_ASSERT_EQUAL_STRING("Hello world", output.c_str());
    TEST_ASSERT_EQUAL(3, FakeTargetPort::delays.size());
    for (const uint32_t delay : FakeTargetPort::delays)
    {
        TEST_ASSERT_EQUAL_UINT32(10, delay);
    }
    TEST_ASSERT_EQUAL(0, PrintUtils::TransmitData());   // nothing left in the buffer
}

/// @brief Nothing is output when nothing was printed
void test_Run_Empty_OutputsNothing(void)
{
    RunLoops(2);
    TEST_ASSERT_EQUAL_STRING("", output.c_str());
}

/// @brief Output printed while the task waits is sent on the next loop, in order
void test_Run_SendsNewOutputOnNextLoop(void)
{
    int loop = 0;
    std::string seenAtDelay[3];
    FakeTargetPort::onDelay = [&]()
    {
        seenAtDelay[loop] = output;
        loop++;
        if (loop == 1)
        {
            Print("second");
        }
    };

    Print("first ");
    RunLoops(3);

    TEST_ASSERT_EQUAL_STRING("first ", seenAtDelay[0].c_str());
    TEST_ASSERT_EQUAL_STRING("first second", seenAtDelay[1].c_str());
    TEST_ASSERT_EQUAL_STRING("first second", output.c_str());
}

/// @brief When the real stdout fails, an error is printed and the data is kept and sent once it recovers
void test_Run_PortError_ReportsAndRetries(void)
{
    writeResult = -1;
    FakeTargetPort::onDelay = []() { writeResult = 0; };   // the port recovers after the first loop

    Print("kept");

    StdoutCapture capture;
    capture.Start();
    RunLoops(2);
    const std::string printed = capture.Stop();

    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "ERROR printing to output stream!"));
    TEST_ASSERT_EQUAL_STRING("kept", output.c_str());
}

/// @brief Run all tests
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Initialize_RedirectsStdOut);
    RUN_TEST(test_Run_SendsBufferThenWaits10ms);
    RUN_TEST(test_Run_Empty_OutputsNothing);
    RUN_TEST(test_Run_SendsNewOutputOnNextLoop);
    RUN_TEST(test_Run_PortError_ReportsAndRetries);
    return UNITY_END();
}
