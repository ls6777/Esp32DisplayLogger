/// @file test_PrintUtils.cpp
/// @brief Host unit tests for PrintUtils.cpp, using a fake StdOutPort.

#include "unity.h"
#include "PrintUtils.hpp"
#include "StdOutPort.hpp"

#include <cstring>

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
    std::memcpy(&output[outputLen], data, static_cast<std::size_t>(len));
    outputLen += len;
    return len;
}

void setUp()
{
    // PrintUtils owns a single buffer for the whole program, so empty it through the fake port
    writeResult = 0;
    PrintUtils::TransmitData();

    redirectCalls = 0;
    outputLen = 0;
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

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_InitStdOut_RedirectsStdOut);
    RUN_TEST(test_Write_BuffersUntilTransmit);
    RUN_TEST(test_TransmitData_Empty_ReturnsZero);
    RUN_TEST(test_TransmitData_PortError_KeepsData);
    RUN_TEST(test_GetHighWaterMark_TracksBufferUsage);
    return UNITY_END();
}
