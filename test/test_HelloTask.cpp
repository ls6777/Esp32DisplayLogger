/// @file test_HelloTask.cpp
/// @brief Host unit tests for HelloTask.cpp.
/// @details Uses the real TaskMonitor and PrintUtils. Only the port layers are fakes: the OS (fakes/TargetPort.hpp) and
///          StdOutPort (defined here).

#include "HelloTask.hpp"

#include "PrintUtils.hpp"
#include "StdOutPort.hpp"
#include "StdoutCapture.hpp"
#include "TargetPort.hpp"
#include "TaskMonitor.hpp"

#include "unity.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    FakeTask helloTask{"HelloTask"};    ///< The task running HelloTask

    QHandle monitorQ = nullptr;     ///< The task monitor's queue
    QHandle helloQ = nullptr;       ///< HelloTask's queue

    /// @brief Run HelloTask for a number of queue reads (each read gets a message, or waits 1 s and says hello)
    /// @param[in] reads - number of queue reads
    /// @return everything printed to stdout
    std::string RunHello(int reads)
    {
        FakeTargetPort::recvBudget = reads;
        FakeTargetPort::currentTask = &helloTask;

        StdoutCapture capture;
        capture.Start();
        HelloTask task;
        try
        {
            task.Run();
        }
        catch (const StopRun&)
        {
        }
        return capture.Stop();
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

    /// @brief Get the numbers of all "Hello world! N" lines
    /// @param[in] printed - captured output
    /// @return the numbers, in order
    std::vector<long> HelloNumbers(const std::string& printed)
    {
        static const std::string HELLO = "Hello world! ";
        std::vector<long> numbers;
        for (size_t pos = printed.find(HELLO); pos != std::string::npos; pos = printed.find(HELLO, pos + 1))
        {
            numbers.push_back(std::strtol(printed.c_str() + pos + HELLO.size(), nullptr, 10));
        }
        return numbers;
    }
}

//------------------------------------------------------------------
// Fake StdOutPort (output is discarded)
//------------------------------------------------------------------
void StdOutPort::Redirect() {}

int StdOutPort::Write(const char* data, int len)
{
    (void)data;
    return len;
}

/// @brief Unity: called before each test. Creates the task monitor's queue, then HelloTask's (holding INITIALIZE).
void setUp(void)
{
    FakeTargetPort::Reset();
    TaskMonitor::Initialize();
    monitorQ = FakeTargetPort::sent.back().queue;

    FakeTargetPort::currentTask = &helloTask;
    HelloTask task;
    task.Initialize();
    helloQ = FakeTargetPort::sent.back().queue;
}

/// @brief Unity: called after each test
void tearDown(void) {}

//------------------------------------------------------------------
// Tests
//------------------------------------------------------------------

/// @brief Initialize creates the task's own queue and posts INITIALIZE to it
void test_Initialize_PostsInitialize(void)
{
    TEST_ASSERT_NOT_EQUAL(monitorQ, helloQ);
    TEST_ASSERT_EQUAL(1, FakeTargetPort::QueueCount(helloQ));
    TEST_ASSERT_EQUAL(Message::INITIALIZE, FakeTargetPort::sent.back().As<Message>().msgId);
}

/// @brief On INITIALIZE the task registers with the monitor: as itself, 5 s timeout, its TaskCheckin callback
void test_HandleInitialize_RegistersWithMonitor(void)
{
    const std::string printed = RunHello(1);

    const std::vector<TaskMonitor::TaskMonitorMessage> msgs = MonitorMessages();
    TEST_ASSERT_EQUAL(2, msgs.size());     // the monitor's own INITIALIZE, then REGISTER
    TEST_ASSERT_EQUAL(Message::REGISTER, msgs[1].msgId);
    TEST_ASSERT_EQUAL_PTR(&helloTask, msgs[1].taskId);
    TEST_ASSERT_EQUAL_UINT32(5000, msgs[1].maxResponseTime);
    TEST_ASSERT_EQUAL_PTR(&HelloTask::TaskCheckin, msgs[1].checkinCall);

    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "HelloTask Initialized"));
}

/// @brief While idle, the task says hello once a second with a counter that goes up by one each time
void test_SaysHelloEverySecond(void)
{
    RunHello(1);    // INITIALIZE
    const uint32_t start = FakeTargetPort::timeMs;

    const std::string printed = RunHello(5);

    const std::vector<long> numbers = HelloNumbers(printed);
    TEST_ASSERT_EQUAL(5, numbers.size());
    for (size_t i = 1; i < numbers.size(); i++)
    {
        TEST_ASSERT_EQUAL(numbers[i - 1] + 1, numbers[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(start + 5000, FakeTargetPort::timeMs);
}

/// @brief Every 10th hello is followed by the printf buffer high water mark
void test_EveryTenthHello_PrintsHighWaterMark(void)
{
    // Put 50 bytes through the printf buffer (output goes to the fake StdOutPort)
    const char data[51] = "01234567890123456789012345678901234567890123456789";
    PrintUtils::Write(data, 50);
    PrintUtils::TransmitData();

    RunHello(1);    // INITIALIZE
    const std::string printed = RunHello(10);

    const std::string mark = "Printf buffer high water mark: 50 of 14999 bytes";
    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, mark));

    // It comes straight after a hello whose number is a multiple of 10
    const std::string before = printed.substr(0, printed.find(mark));
    const std::vector<long> numbers = HelloNumbers(before);
    TEST_ASSERT_FALSE(numbers.empty());
    TEST_ASSERT_EQUAL(0, numbers.back() % 10);
}

/// @brief TaskCheckin (the callback the monitor calls) posts TASK_CHECKIN to the task's own queue
void test_TaskCheckin_PostsToOwnQueue(void)
{
    RunHello(1);    // INITIALIZE
    HelloTask::TaskCheckin();

    TEST_ASSERT_EQUAL_PTR(helloQ, FakeTargetPort::sent.back().queue);
    TEST_ASSERT_EQUAL(Message::TASK_CHECKIN, FakeTargetPort::sent.back().As<Message>().msgId);
}

/// @brief A checkin request is answered straight away, as the task, without waiting for the hello timer
void test_CheckinRequest_ChecksInWithMonitor(void)
{
    RunHello(1);    // INITIALIZE

    // Ask through the callback registered with the monitor, as the monitor would
    const TaskMonitor::CheckinCall callback = MonitorMessages().back().checkinCall;
    callback();

    const uint32_t start = FakeTargetPort::timeMs;
    const std::string printed = RunHello(1);

    const TaskMonitor::TaskMonitorMessage last = MonitorMessages().back();
    TEST_ASSERT_EQUAL(Message::TASK_CHECKIN, last.msgId);
    TEST_ASSERT_EQUAL_PTR(&helloTask, last.taskId);
    TEST_ASSERT_EQUAL_UINT32(start, FakeTargetPort::timeMs);
    TEST_ASSERT_EQUAL(0, HelloNumbers(printed).size());
}

/// @brief Unknown messages are reported and ignored
void test_UnknownMessage_Reported(void)
{
    RunHello(1);    // INITIALIZE
    const Message bad(Message::NUM_MSG_IDS);
    TEST_ASSERT_EQUAL(PASS, TargetPort::QSend(helloQ, &bad));

    const std::string printed = RunHello(1);
    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "HelloTask - unknown msgId"));
}

/// @brief Shutdown prints a message and then blocks forever
void test_Shutdown_BlocksForever(void)
{
    RunHello(1);    // INITIALIZE
    HelloTask task;
    task.Shutdown();

    const std::string printed = RunHello(5);

    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "HelloTask Shutdown"));
    TEST_ASSERT_EQUAL(1, FakeTargetPort::delays.size());
    TEST_ASSERT_EQUAL_UINT32(MAX_DELAY, FakeTargetPort::delays[0]);
}

/// @brief Run all tests
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Initialize_PostsInitialize);
    RUN_TEST(test_HandleInitialize_RegistersWithMonitor);
    RUN_TEST(test_SaysHelloEverySecond);
    RUN_TEST(test_EveryTenthHello_PrintsHighWaterMark);
    RUN_TEST(test_TaskCheckin_PostsToOwnQueue);
    RUN_TEST(test_CheckinRequest_ChecksInWithMonitor);
    RUN_TEST(test_UnknownMessage_Reported);
    RUN_TEST(test_Shutdown_BlocksForever);
    return UNITY_END();
}
