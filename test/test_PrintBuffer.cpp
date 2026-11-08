/// @file test_PrintBuffer.cpp
/// @brief Host unit tests for PrintBuffer.hpp.

#include "unity.h"
#include "PrintBuffer.hpp"

#include <cstring>

// Small buffer so wrap-around and full-buffer cases are easy to exercise. Usable capacity is 15 bytes.
constexpr int SIZE = 16;
using TestBuffer = PrintBuffer<SIZE>;
constexpr int CAPACITY = TestBuffer::CAPACITY;

/// Buffer under test, recreated before each test
static TestBuffer buffer;

//------------------------------------------------------------------
// Fake output function
//------------------------------------------------------------------
static char output[1024];
static int outputLen;
static int outputCalls;
static int outputLimit;   // max bytes accepted per call, < 0 for unlimited
static int outputResult;  // if non-zero, returned instead of writing (e.g. an error)

static int FakeOutput(const char* data, int len)
{
    outputCalls++;
    TEST_ASSERT_GREATER_THAN_INT(0, len);

    if (outputResult != 0)
    {
        return outputResult;
    }

    if ((outputLimit >= 0) && (len > outputLimit))
    {
        len = outputLimit;
    }
    std::memcpy(&output[outputLen], data, static_cast<std::size_t>(len));
    outputLen += len;
    return len;
}

static void AssertOutput(const char* expected)
{
    TEST_ASSERT_EQUAL_INT(static_cast<int>(std::strlen(expected)), outputLen);
    TEST_ASSERT_EQUAL_MEMORY(expected, output, static_cast<std::size_t>(outputLen));
}

static void ClearOutput()
{
    outputLen = 0;
    outputCalls = 0;
}

void setUp()
{
    buffer = TestBuffer();
    ClearOutput();
    outputLimit = -1;
    outputResult = 0;
}

void tearDown()
{
}

//------------------------------------------------------------------
// Tests
//------------------------------------------------------------------
static void test_Empty_TransmitDoesNotCallOutput()
{
    TEST_ASSERT_EQUAL_INT(0, buffer.GetOccupiedSize());
    TEST_ASSERT_EQUAL_INT(0, buffer.Transmit(FakeOutput));
    TEST_ASSERT_EQUAL_INT(0, outputCalls);
}

static void test_Write_ThenTransmit_OutputsData()
{
    TEST_ASSERT_EQUAL_INT(5, buffer.Write("hello", 5));
    TEST_ASSERT_EQUAL_INT(5, buffer.GetOccupiedSize());

    TEST_ASSERT_EQUAL_INT(5, buffer.Transmit(FakeOutput));
    AssertOutput("hello");
    TEST_ASSERT_EQUAL_INT(1, outputCalls);
    TEST_ASSERT_EQUAL_INT(0, buffer.GetOccupiedSize());
}

static void test_MultipleWrites_AreOutputInOrder()
{
    buffer.Write("ab", 2);
    buffer.Write("cd", 2);
    buffer.Write("ef", 2);

    TEST_ASSERT_EQUAL_INT(6, buffer.Transmit(FakeOutput));
    AssertOutput("abcdef");
}

static void test_Write_InvalidArgs_WritesNothing()
{
    TEST_ASSERT_EQUAL_INT(0, buffer.Write(nullptr, 5));
    TEST_ASSERT_EQUAL_INT(0, buffer.Write("abc", 0));
    TEST_ASSERT_EQUAL_INT(0, buffer.Write("abc", -1));
    TEST_ASSERT_EQUAL_INT(0, buffer.GetOccupiedSize());
}

static void test_Write_FillsToCapacity()
{
    const char data[] = "0123456789ABCDE"; // 15 bytes == CAPACITY
    TEST_ASSERT_EQUAL_INT(CAPACITY, buffer.Write(data, CAPACITY));
    TEST_ASSERT_EQUAL_INT(CAPACITY, buffer.GetOccupiedSize());

    TEST_ASSERT_EQUAL_INT(CAPACITY, buffer.Transmit(FakeOutput));
    AssertOutput(data);
}

static void test_Write_WhenTooLarge_TruncatesToCapacity()
{
    const char data[] = "0123456789ABCDEFGHIJ"; // 20 bytes
    TEST_ASSERT_EQUAL_INT(CAPACITY, buffer.Write(data, 20));

    buffer.Transmit(FakeOutput);
    AssertOutput("0123456789ABCDE");
}

static void test_Write_WhenFull_WritesNothing()
{
    buffer.Write("0123456789ABCDE", CAPACITY);
    TEST_ASSERT_EQUAL_INT(0, buffer.Write("X", 1));
    TEST_ASSERT_EQUAL_INT(CAPACITY, buffer.GetOccupiedSize());
}

static void test_Write_ExactlyToEndOfBuffer()
{
    // move head/tail to index 6, then write exactly the 10 bytes up to the end of the buffer
    buffer.Write("xxxxxx", 6);
    buffer.Transmit(FakeOutput);
    ClearOutput();

    TEST_ASSERT_EQUAL_INT(10, buffer.Write("0123456789", 10));
    TEST_ASSERT_EQUAL_INT(10, buffer.GetOccupiedSize());

    TEST_ASSERT_EQUAL_INT(10, buffer.Transmit(FakeOutput));
    AssertOutput("0123456789");
    TEST_ASSERT_EQUAL_INT(1, outputCalls);
    TEST_ASSERT_EQUAL_INT(0, buffer.GetOccupiedSize());

    // next write starts back at the beginning of the buffer
    buffer.Write("ok", 2);
    ClearOutput();
    buffer.Transmit(FakeOutput);
    AssertOutput("ok");
}

static void test_Write_WrapsAroundEnd()
{
    buffer.Write("0123456789", 10);
    buffer.Transmit(FakeOutput);
    ClearOutput();

    // 6 bytes fit before the end, 4 wrap to the start
    TEST_ASSERT_EQUAL_INT(10, buffer.Write("ABCDEFGHIJ", 10));
    TEST_ASSERT_EQUAL_INT(10, buffer.GetOccupiedSize());

    // wrapped data is output in a single transmit, using two output calls
    TEST_ASSERT_EQUAL_INT(10, buffer.Transmit(FakeOutput));
    AssertOutput("ABCDEFGHIJ");
    TEST_ASSERT_EQUAL_INT(2, outputCalls);
    TEST_ASSERT_EQUAL_INT(0, buffer.GetOccupiedSize());
}

static void test_Write_WhenWrapped_LimitedBySpace()
{
    // head at 10, tail wraps to 4 -> 10 bytes used, 5 free
    buffer.Write("0123456789", 10);
    buffer.Transmit(FakeOutput);
    buffer.Write("ABCDEFGHIJ", 10);
    ClearOutput();

    TEST_ASSERT_EQUAL_INT(5, buffer.Write("vwxyz!!", 7));
    TEST_ASSERT_EQUAL_INT(CAPACITY, buffer.GetOccupiedSize());

    buffer.Transmit(FakeOutput);
    AssertOutput("ABCDEFGHIJvwxyz");
}

static void test_Transmit_PartialOutput_KeepsRemainder()
{
    buffer.Write("abcdef", 6);

    outputLimit = 4;
    TEST_ASSERT_EQUAL_INT(4, buffer.Transmit(FakeOutput));
    AssertOutput("abcd");
    TEST_ASSERT_EQUAL_INT(1, outputCalls);
    TEST_ASSERT_EQUAL_INT(2, buffer.GetOccupiedSize());

    outputLimit = -1;
    TEST_ASSERT_EQUAL_INT(2, buffer.Transmit(FakeOutput));
    AssertOutput("abcdef");
}

static void test_Transmit_ZeroOutput_KeepsData()
{
    buffer.Write("abc", 3);

    outputLimit = 0;
    TEST_ASSERT_EQUAL_INT(0, buffer.Transmit(FakeOutput));
    TEST_ASSERT_EQUAL_INT(1, outputCalls);
    TEST_ASSERT_EQUAL_INT(3, buffer.GetOccupiedSize());
}

static void test_Transmit_Error_ReturnsErrorAndKeepsData()
{
    buffer.Write("abc", 3);

    outputResult = -1;
    TEST_ASSERT_EQUAL_INT(-1, buffer.Transmit(FakeOutput));
    TEST_ASSERT_EQUAL_INT(3, buffer.GetOccupiedSize());

    outputResult = 0;
    TEST_ASSERT_EQUAL_INT(3, buffer.Transmit(FakeOutput));
    AssertOutput("abc");
}

static void test_HighWaterMark_TracksMaximum()
{
    TEST_ASSERT_EQUAL_INT(0, buffer.GetHighWaterMark());

    buffer.Write("abcdefgh", 8);
    TEST_ASSERT_EQUAL_INT(8, buffer.GetHighWaterMark());

    // transmitting does not lower the high water mark
    buffer.Transmit(FakeOutput);
    TEST_ASSERT_EQUAL_INT(8, buffer.GetHighWaterMark());

    // smaller usage doesn't change it, larger usage does (including across the wrap)
    buffer.Write("abc", 3);
    TEST_ASSERT_EQUAL_INT(8, buffer.GetHighWaterMark());
    buffer.Write("defghijk", 8);
    TEST_ASSERT_EQUAL_INT(11, buffer.GetHighWaterMark());
}

static void test_Instances_AreIndependent()
{
    PrintBuffer<8> other;
    TEST_ASSERT_EQUAL_INT(7, PrintBuffer<8>::CAPACITY);

    buffer.Write("abc", 3);
    TEST_ASSERT_EQUAL_INT(0, other.GetOccupiedSize());
    TEST_ASSERT_EQUAL_INT(0, other.GetHighWaterMark());

    other.Write("xyz", 3);
    TEST_ASSERT_EQUAL_INT(3, other.Transmit(FakeOutput));
    AssertOutput("xyz");
    TEST_ASSERT_EQUAL_INT(3, buffer.GetOccupiedSize());
}

static void test_ManyCycles_OutputMatchesInput()
{
    // Write varying sized chunks and transmit with varying output limits, so the head and tail wrap
    // many times at different positions. Everything accepted must come out in order, unchanged.
    char expected[sizeof(output)];
    int expectedLen = 0;
    unsigned int seed = 12345;
    char nextChar = 'a';

    outputLimit = 0;
    for (int i = 0; i < 200; i++)
    {
        seed = seed * 1103515245u + 12345u;
        const int writeLen = static_cast<int>((seed >> 16) % 9);     // 0..8 bytes
        const int limit = static_cast<int>((seed >> 8) % 7);         // 0..6 bytes per output call

        char chunk[8];
        for (int j = 0; j < writeLen; j++)
        {
            chunk[j] = nextChar;
            nextChar = (nextChar == 'z') ? 'a' : static_cast<char>(nextChar + 1);
        }

        const int written = buffer.Write(chunk, writeLen);
        TEST_ASSERT_LESS_OR_EQUAL_INT(CAPACITY, buffer.GetOccupiedSize());

        // dropped bytes are lost, so rewind the generator to keep expected data contiguous
        for (int j = written; j < writeLen; j++)
        {
            nextChar = (nextChar == 'a') ? 'z' : static_cast<char>(nextChar - 1);
        }
        std::memcpy(&expected[expectedLen], chunk, static_cast<std::size_t>(written));
        expectedLen += written;

        outputLimit = limit;
        buffer.Transmit(FakeOutput);

        if (outputLen > static_cast<int>(sizeof(output)) - 2 * SIZE)
        {
            break; // stay within the capture buffer
        }
    }

    outputLimit = -1;
    buffer.Transmit(FakeOutput);
    TEST_ASSERT_EQUAL_INT(0, buffer.GetOccupiedSize());
    TEST_ASSERT_EQUAL_INT(expectedLen, outputLen);
    TEST_ASSERT_EQUAL_MEMORY(expected, output, static_cast<std::size_t>(expectedLen));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_Empty_TransmitDoesNotCallOutput);
    RUN_TEST(test_Write_ThenTransmit_OutputsData);
    RUN_TEST(test_MultipleWrites_AreOutputInOrder);
    RUN_TEST(test_Write_InvalidArgs_WritesNothing);
    RUN_TEST(test_Write_FillsToCapacity);
    RUN_TEST(test_Write_WhenTooLarge_TruncatesToCapacity);
    RUN_TEST(test_Write_WhenFull_WritesNothing);
    RUN_TEST(test_Write_ExactlyToEndOfBuffer);
    RUN_TEST(test_Write_WrapsAroundEnd);
    RUN_TEST(test_Write_WhenWrapped_LimitedBySpace);
    RUN_TEST(test_Transmit_PartialOutput_KeepsRemainder);
    RUN_TEST(test_Transmit_ZeroOutput_KeepsData);
    RUN_TEST(test_Transmit_Error_ReturnsErrorAndKeepsData);
    RUN_TEST(test_HighWaterMark_TracksMaximum);
    RUN_TEST(test_Instances_AreIndependent);
    RUN_TEST(test_ManyCycles_OutputMatchesInput);
    return UNITY_END();
}
