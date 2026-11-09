/// @file test_CpuUsage.cpp
/// @brief Host unit tests for CpuUsage (the target independent CPU usage calculation).

#include "CpuUsage.hpp"

#include "unity.h"

/// @brief Unity: called before each test
void setUp(void) {}

/// @brief Unity: called after each test
void tearDown(void) {}

/// @brief Before any reading, and after only the first, there's no value yet
void test_FirstReading_OnlySetsStart(void)
{
    CpuUsage cpu;
    TEST_ASSERT_FALSE(cpu.HasValue());
    TEST_ASSERT_EQUAL_UINT32(0, cpu.GetPercent());

    TEST_ASSERT_EQUAL_UINT32(0, cpu.Update(123456, 999999));
    TEST_ASSERT_FALSE(cpu.HasValue());
}

/// @brief Usage is the busy share of the interval since the previous reading
void test_UsageOverInterval(void)
{
    CpuUsage cpu;
    cpu.Update(1000, 2000);

    TEST_ASSERT_EQUAL_UINT32(25, cpu.Update(1000 + 750, 2000 + 1000));     // 750 of 1000 idle
    TEST_ASSERT_TRUE(cpu.HasValue());
    TEST_ASSERT_EQUAL_UINT32(25, cpu.GetPercent());

    TEST_ASSERT_EQUAL_UINT32(90, cpu.Update(1750 + 100, 3000 + 1000));     // only the new interval counts
}

/// @brief All idle is 0 %, no idle is 100 %
void test_Limits(void)
{
    CpuUsage cpu;
    cpu.Update(0, 0);
    TEST_ASSERT_EQUAL_UINT32(0, cpu.Update(1000, 1000));
    TEST_ASSERT_EQUAL_UINT32(100, cpu.Update(1000, 2000));
}

/// @brief The usage is rounded to the nearest percent
void test_Rounding(void)
{
    CpuUsage cpu;
    cpu.Update(0, 0);
    TEST_ASSERT_EQUAL_UINT32(33, cpu.Update(2, 3));                     // 1/3 busy = 33.3 %
    TEST_ASSERT_EQUAL_UINT32(67, cpu.Update(2 + 1, 3 + 3));             // 2/3 busy = 66.7 %
    TEST_ASSERT_EQUAL_UINT32(1, cpu.Update(3 + 199, 6 + 200));          // 0.5 % rounds up
    TEST_ASSERT_EQUAL_UINT32(0, cpu.Update(202 + 1999, 206 + 2000));    // 0.05 % rounds down
}

/// @brief If the idle counter appears to run ahead of the total (it can lag and catch up), the usage is 0, not negative
void test_IdleAheadOfTotal_IsZero(void)
{
    CpuUsage cpu;
    cpu.Update(0, 0);
    TEST_ASSERT_EQUAL_UINT32(0, cpu.Update(1100, 1000));
}

/// @brief If no time passed, the previous usage is kept
void test_NoTimePassed_KeepsPrevious(void)
{
    CpuUsage cpu;
    cpu.Update(0, 0);
    cpu.Update(500, 1000);  // 50 %
    TEST_ASSERT_EQUAL_UINT32(50, cpu.Update(500, 1000));
    TEST_ASSERT_TRUE(cpu.HasValue());
}

/// @brief No time between the first two readings: still no value
void test_NoTimePassed_BeforeFirstValue(void)
{
    CpuUsage cpu;
    cpu.Update(10, 20);
    TEST_ASSERT_EQUAL_UINT32(0, cpu.Update(10, 20));
    TEST_ASSERT_FALSE(cpu.HasValue());
}

/// @brief Counters that wrap around 2^32 still give the right usage
void test_CountersWrap(void)
{
    CpuUsage cpu;
    cpu.Update(0xFFFFFF00u, 0xFFFFFE00u);
    // 1000 total later (wrapped), 400 idle later (wrapped): 60 % busy
    TEST_ASSERT_EQUAL_UINT32(60, cpu.Update(0xFFFFFF00u + 400u, 0xFFFFFE00u + 1000u));
}

/// @brief Large intervals don't overflow the calculation (busy * 100 exceeds 32 bits)
void test_LargeInterval(void)
{
    CpuUsage cpu;
    cpu.Update(0, 0);
    TEST_ASSERT_EQUAL_UINT32(75, cpu.Update(1000000000u, 4000000000u));
}

/// @brief Run all tests
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_FirstReading_OnlySetsStart);
    RUN_TEST(test_UsageOverInterval);
    RUN_TEST(test_Limits);
    RUN_TEST(test_Rounding);
    RUN_TEST(test_IdleAheadOfTotal_IsZero);
    RUN_TEST(test_NoTimePassed_KeepsPrevious);
    RUN_TEST(test_NoTimePassed_BeforeFirstValue);
    RUN_TEST(test_CountersWrap);
    RUN_TEST(test_LargeInterval);
    return UNITY_END();
}
