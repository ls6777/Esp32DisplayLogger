/// @file test_PrintUtils.cpp
/// @brief Host unit tests for PrintUtils.cpp, using a fake StdOutPort.

#include "unity.h"
#include "PrintUtils.hpp"
#include "StdOutPort.hpp"

#include <cstring>
#include <string>

//------------------------------------------------------------------
// Fake StdOutPort
//------------------------------------------------------------------
static int redirectCalls;
static char output[256];
static int outputLen;
static int writeResult; // if non-zero, returned instead of writing (e.g. an error)

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
    // Keep only what fits in output[] (some tests send far more), but accept it all
    const int space = static_cast<int>(sizeof(output)) - outputLen;
    const int keep = (len < space) ? len : space;
    std::memcpy(&output[outputLen], data, static_cast<std::size_t>(keep));
    outputLen += keep;
    return len;
}

//------------------------------------------------------------------
// Mirror
//------------------------------------------------------------------
static std::string mirrored;    // everything passed to the mirror
static int mirrorCalls;         // number of mirror calls

static void Mirror(const char* data, int len)
{
    mirrored.append(data, static_cast<std::size_t>(len));
    mirrorCalls++;
}

void setUp()
{
    // PrintUtils owns a single buffer for the whole program, so empty it through the fake port
    PrintUtils::SetMirror(nullptr);
    writeResult = 0;
    PrintUtils::TransmitData();

    redirectCalls = 0;
    outputLen = 0;
    mirrored.clear();
    mirrorCalls = 0;
}

void tearDown()
{
}

//------------------------------------------------------------------
// Tests
//------------------------------------------------------------------
static void test_InitStdOut_RedirectsStdOut()
{
    PrintUtils::InitStdOut();
    TEST_ASSERT_EQUAL_INT(1, redirectCalls);
}

static void test_Write_BuffersUntilTransmit()
{
    TEST_ASSERT_EQUAL_INT(5, PrintUtils::Write("hello", 5));
    TEST_ASSERT_EQUAL_INT(0, outputLen);

    TEST_ASSERT_EQUAL_INT(5, PrintUtils::TransmitData());
    TEST_ASSERT_EQUAL_INT(5, outputLen);
    TEST_ASSERT_EQUAL_MEMORY("hello", output, 5);
}

static void test_TransmitData_Empty_ReturnsZero()
{
    TEST_ASSERT_EQUAL_INT(0, PrintUtils::TransmitData());
    TEST_ASSERT_EQUAL_INT(0, outputLen);
}

static void test_TransmitData_PortError_KeepsData()
{
    PrintUtils::Write("hello", 5);

    writeResult = -1;
    TEST_ASSERT_EQUAL_INT(-1, PrintUtils::TransmitData());
    TEST_ASSERT_EQUAL_INT(0, outputLen);

    writeResult = 0;
    TEST_ASSERT_EQUAL_INT(5, PrintUtils::TransmitData());
    TEST_ASSERT_EQUAL_MEMORY("hello", output, 5);
}

static void test_GetHighWaterMark_TracksBufferUsage()
{
    // the high water mark never decreases, so write more than any earlier test has
    char data[64];
    std::memset(data, 'x', sizeof(data));
    const int len = PrintUtils::GetHighWaterMark() + 10;
    TEST_ASSERT_LESS_OR_EQUAL_INT(static_cast<int>(sizeof(data)), len);

    PrintUtils::Write(data, len);
    PrintUtils::TransmitData();

    TEST_ASSERT_EQUAL_INT(len, PrintUtils::GetHighWaterMark());
}

// The mirror gets exactly what was transmitted, in order, and only when transmitted
static void test_Mirror_GetsTransmittedData()
{
    PrintUtils::SetMirror(Mirror);
    PrintUtils::Write("hello ", 6);
    TEST_ASSERT_EQUAL_INT(0, mirrorCalls);      // not before it is transmitted

    PrintUtils::Write("world", 5);
    PrintUtils::TransmitData();
    TEST_ASSERT_EQUAL_STRING("hello world", mirrored.c_str());
    TEST_ASSERT_EQUAL_MEMORY(output, mirrored.data(), mirrored.size());
}

// Data that wraps around the end of the buffer is transmitted in two pieces; the mirror gets both, in order
static void test_Mirror_WrappedData()
{
    PrintUtils::SetMirror(Mirror);

    // The buffer position left by earlier tests is unknown, so send a 7 byte message until one send wraps around the
    // end (shows up as two mirror calls). The buffer size isn't a multiple of 7, so this happens within one pass.
    static_assert((PrintUtils::BUFFER_SIZE % 7) != 0, "the message must not divide the buffer evenly");
    bool wrapped = false;
    for (int i = 0; (i <= (PrintUtils::BUFFER_SIZE / 7) + 1) && !wrapped; i++)
    {
        mirrored.clear();
        mirrorCalls = 0;
        outputLen = 0;
        PrintUtils::Write("abcdefg", 7);
        PrintUtils::TransmitData();
        wrapped = (mirrorCalls == 2);
    }

    TEST_ASSERT_TRUE_MESSAGE(wrapped, "no send wrapped around the buffer end");
    TEST_ASSERT_EQUAL_STRING("abcdefg", mirrored.c_str());
}

// If the real stdout only takes part of the data, the mirror only gets that part
static void test_Mirror_PartialWrite_OnlyAcceptedData()
{
    PrintUtils::SetMirror(Mirror);
    PrintUtils::Write("hello", 5);

    writeResult = 3;    // the port accepts 3 bytes
    PrintUtils::TransmitData();
    TEST_ASSERT_EQUAL_STRING("hel", mirrored.c_str());

    writeResult = 0;    // the rest goes on the next transmit
    PrintUtils::TransmitData();
    TEST_ASSERT_EQUAL_STRING("hello", mirrored.c_str());
}

// If the real stdout fails, the mirror gets nothing (the data is sent, and mirrored, later)
static void test_Mirror_PortError_NotCalled()
{
    PrintUtils::SetMirror(Mirror);
    PrintUtils::Write("hello", 5);

    writeResult = -1;
    PrintUtils::TransmitData();
    TEST_ASSERT_EQUAL_INT(0, mirrorCalls);

    writeResult = 0;
    PrintUtils::TransmitData();
    TEST_ASSERT_EQUAL_STRING("hello", mirrored.c_str());
}

// Clearing the mirror stops the copies; output still goes to stdout
static void test_Mirror_Cleared_NotCalled()
{
    PrintUtils::SetMirror(Mirror);
    PrintUtils::SetMirror(nullptr);
    PrintUtils::Write("hello", 5);
    TEST_ASSERT_EQUAL_INT(5, PrintUtils::TransmitData());
    TEST_ASSERT_EQUAL_INT(0, mirrorCalls);
    TEST_ASSERT_EQUAL_INT(5, outputLen);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_InitStdOut_RedirectsStdOut);
    RUN_TEST(test_Write_BuffersUntilTransmit);
    RUN_TEST(test_TransmitData_Empty_ReturnsZero);
    RUN_TEST(test_TransmitData_PortError_KeepsData);
    RUN_TEST(test_GetHighWaterMark_TracksBufferUsage);
    RUN_TEST(test_Mirror_GetsTransmittedData);
    RUN_TEST(test_Mirror_WrappedData);
    RUN_TEST(test_Mirror_PartialWrite_OnlyAcceptedData);
    RUN_TEST(test_Mirror_PortError_NotCalled);
    RUN_TEST(test_Mirror_Cleared_NotCalled);
    return UNITY_END();
}
