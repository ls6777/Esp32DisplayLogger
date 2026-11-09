/// @file test_TaskMonitor.cpp
/// @brief Host unit tests for TaskMonitor.cpp (the task monitor task), using the fake OS port.
/// @details TaskMonitor.cpp keeps its state (registered tasks, violation flag, last evaluation time) in static
///          variables for the life of the program, so the tests depend on their order:
///          - each test parks the tasks it registered (re-registers them with a huge timeout) before it ends
///          - the violation test runs last, because a violation can't be cleared

#include "StdoutCapture.hpp"
#include "TargetPort.hpp"
#include "TaskMonitor.hpp"

#include "unity.h"

#include <string>

namespace
{
    constexpr uint32_t EVAL_PERIOD = 10;            ///< Task monitor evaluation period (ms), see TaskMonitor.cpp
    constexpr uint32_t PARKED_TIMEOUT = 0x7FFFFFFF; ///< Timeout that never expires within the tests

    FakeTask taskA{"TaskA"};    ///< Task used by test_CheckinRequest_AfterTimeoutAndOnce
    FakeTask taskB{"TaskB"};    ///< Task used by test_UnsolicitedCheckins_PreventRequests
    FakeTask taskC{"TaskC"};    ///< Task used by test_MaxResponseTimes_PrintedEvery30s
    FakeTask taskD{"TaskD"};    ///< Task used by test_Violation_LogsOnceAndStopsKickingWatchdog

    int requestsA = 0;          ///< Checkin requests sent to taskA
    int requestsB = 0;          ///< Checkin requests sent to taskB
    int requestsC = 0;          ///< Checkin requests sent to taskC
    int requestsD = 0;          ///< Checkin requests sent to taskD
    bool autoRespondC = false;  ///< When true, taskC checks in as soon as it is asked

    QHandle monitorQ = nullptr; ///< The task monitor's queue (found from the INITIALIZE message)

    /// @brief Checkin callback for taskA
    void RequestA() { requestsA++; }

    /// @brief Checkin callback for taskB
    void RequestB() { requestsB++; }

    /// @brief Checkin callback for taskD
    void RequestD() { requestsD++; }

    /// @brief Checkin callback for taskC: counts, and checks in straight away when autoRespondC is set
    void RequestC()
    {
        requestsC++;
        if (autoRespondC)
        {
            FakeTargetPort::currentTask = &taskC;
            TaskMonitor::TaskCheckin();
        }
    }

    /// @brief Run one loop of the task monitor: get one message or wait one period, then evaluate
    void Step()
    {
        FakeTargetPort::recvBudget = 1;
        TaskMonitor monitor;
        try
        {
            monitor.Run();
        }
        catch (const StopRun&)
        {
        }
    }

    /// @brief Run the task monitor until all queued messages are processed
    void Drain()
    {
        while (FakeTargetPort::QueueCount(monitorQ) > 0)
        {
            Step();
        }
    }

    /// @brief Run the task monitor until the fake clock has advanced by a time
    /// @param[in] ms - time to run for, in ms
    void RunForMs(uint32_t ms)
    {
        const uint32_t end = FakeTargetPort::timeMs + ms;
        while (FakeTargetPort::timeMs < end)
        {
            Step();
        }
    }

    /// @brief Register a task with the monitor, as that task, and process the registration
    /// @param[in] task - task registering
    /// @param[in] timeout - checkin timeout (ms)
    /// @param[in] callback - checkin request callback
    void RegisterAs(FakeTask* task, uint32_t timeout, TaskMonitor::CheckinCall callback)
    {
        FakeTargetPort::currentTask = task;
        TaskMonitor::Register(timeout, callback);
        Drain();
    }

    /// @brief Check in with the monitor, as a task, and process the checkin
    /// @param[in] task - task checking in
    void CheckinAs(FakeTask* task)
    {
        FakeTargetPort::currentTask = task;
        TaskMonitor::TaskCheckin();
        Drain();
    }

    /// @brief Get the last message sent to the task monitor
    /// @return the message
    TaskMonitor::TaskMonitorMessage LastMessage()
    {
        TEST_ASSERT_FALSE(FakeTargetPort::sent.empty());
        return FakeTargetPort::sent.back().As<TaskMonitor::TaskMonitorMessage>();
    }
}

/// @brief Unity: called before each test. Gives the monitor a new queue holding the INITIALIZE message.
void setUp(void)
{
    FakeTargetPort::Reset();
    TaskMonitor::Initialize();
    TEST_ASSERT_EQUAL(1, FakeTargetPort::sent.size());
    monitorQ = FakeTargetPort::sent[0].queue;
}

/// @brief Unity: called after each test
void tearDown(void) {}

//------------------------------------------------------------------
// Tests
//------------------------------------------------------------------

/// @brief Initialize posts INITIALIZE; processing it starts the watchdog, then every loop kicks it
void test_Initialize_StartsAndKicksWatchdog(void)
{
    TEST_ASSERT_EQUAL(Message::INITIALIZE, LastMessage().msgId);

    StdoutCapture capture;
    capture.Start();
    Step();                 // INITIALIZE
    RunForMs(50);           // 5 evaluation periods
    const std::string printed = capture.Stop();

    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "Task Monitor Initialized"));
    TEST_ASSERT_EQUAL(1, FakeTargetPort::watchdogStarts);
    TEST_ASSERT_EQUAL(6, FakeTargetPort::watchdogResets);
    TEST_ASSERT_EQUAL(0, FakeTargetPort::errorsLogged);
}

/// @brief The monitor waits one evaluation period (10 ms) for each message
void test_Run_EvaluatesEvery10ms(void)
{
    Drain();
    const uint32_t start = FakeTargetPort::timeMs;
    Step();
    TEST_ASSERT_EQUAL_UINT32(start + EVAL_PERIOD, FakeTargetPort::timeMs);
}

/// @brief Register sends the calling task, its timeout and its callback
void test_Register_SendsCallingTask(void)
{
    FakeTargetPort::currentTask = &taskA;
    TaskMonitor::Register(1234, &RequestA);

    const TaskMonitor::TaskMonitorMessage msg = LastMessage();
    TEST_ASSERT_EQUAL(Message::REGISTER, msg.msgId);
    TEST_ASSERT_EQUAL_PTR(&taskA, msg.taskId);
    TEST_ASSERT_EQUAL_UINT32(1234, msg.maxResponseTime);
    TEST_ASSERT_EQUAL_PTR(&RequestA, msg.checkinCall);
    // Not processed: setUp() of the next test discards the queue
}

/// @brief TaskCheckin sends the calling task
void test_TaskCheckin_SendsCallingTask(void)
{
    FakeTargetPort::currentTask = &taskB;
    TaskMonitor::TaskCheckin();

    const TaskMonitor::TaskMonitorMessage msg = LastMessage();
    TEST_ASSERT_EQUAL(Message::TASK_CHECKIN, msg.msgId);
    TEST_ASSERT_EQUAL_PTR(&taskB, msg.taskId);
}

/// @brief A checkin is requested only once the timeout has passed, only once per wait, and again after a response
void test_CheckinRequest_AfterTimeoutAndOnce(void)
{
    Drain();
    RegisterAs(&taskA, 100, &RequestA);

    RunForMs(100);                      // exactly the timeout: not late yet
    TEST_ASSERT_EQUAL(0, requestsA);

    RunForMs(EVAL_PERIOD);              // past the timeout: request
    TEST_ASSERT_EQUAL(1, requestsA);

    RunForMs(90);                       // still waiting for the response: no second request, no violation
    TEST_ASSERT_EQUAL(1, requestsA);
    TEST_ASSERT_EQUAL(0, FakeTargetPort::errorsLogged);

    CheckinAs(&taskA);                  // response restarts the period
    RunForMs(100);
    TEST_ASSERT_EQUAL(1, requestsA);
    RunForMs(EVAL_PERIOD);
    TEST_ASSERT_EQUAL(2, requestsA);

    CheckinAs(&taskA);
    RegisterAs(&taskA, PARKED_TIMEOUT, &RequestA);
}

/// @brief A task that checks in more often than its timeout is never asked to check in
void test_UnsolicitedCheckins_PreventRequests(void)
{
    Drain();
    RegisterAs(&taskB, 100, &RequestB);

    for (int i = 0; i < 20; i++)
    {
        RunForMs(50);
        CheckinAs(&taskB);
    }

    TEST_ASSERT_EQUAL(0, requestsB);
    TEST_ASSERT_EQUAL(0, FakeTargetPort::errorsLogged);
    RegisterAs(&taskB, PARKED_TIMEOUT, &RequestB);
}

/// @brief Registering again resets the task, including any request in progress
void test_Register_AgainResetsTask(void)
{
    Drain();
    RegisterAs(&taskA, 100, &RequestA);
    const int before = requestsA;

    RunForMs(110);                      // request sent
    TEST_ASSERT_EQUAL(before + 1, requestsA);

    RegisterAs(&taskA, 100, &RequestA); // reset: the wait for the response is cancelled
    RunForMs(100);
    TEST_ASSERT_EQUAL(before + 1, requestsA);
    TEST_ASSERT_EQUAL(0, FakeTargetPort::errorsLogged);

    RegisterAs(&taskA, PARKED_TIMEOUT, &RequestA);
}

/// @brief Unknown messages are reported and ignored
void test_UnknownMessage_Reported(void)
{
    Drain();
    const TaskMonitor::TaskMonitorMessage bad(Message::NUM_MSG_IDS);
    TEST_ASSERT_EQUAL(PASS, TargetPort::QSend(monitorQ, &bad));

    StdoutCapture capture;
    capture.Start();
    Drain();
    const std::string printed = capture.Stop();

    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "TaskMonitor - unknown msgId"));
}

/// @brief Every 30 s of running, the longest response time of each registered task is printed
void test_MaxResponseTimes_PrintedEvery30s(void)
{
    Drain();
    RegisterAs(&taskC, 100, &RequestC);

    RunForMs(110);                      // request
    TEST_ASSERT_EQUAL(1, requestsC);
    RunForMs(50);                       // respond 50 ms later
    CheckinAs(&taskC);

    // From now on taskC responds straight away, so it never violates during the long run
    autoRespondC = true;

    // One long run (the 30 s period restarts each time Run() is called)
    FakeTargetPort::recvBudget = 3400;  // over 3000 evaluation periods, plus the checkin messages
    StdoutCapture capture;
    capture.Start();
    TaskMonitor monitor;
    try
    {
        monitor.Run();
    }
    catch (const StopRun&)
    {
    }
    const std::string printed = capture.Stop();

    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "MAX time = 50 ms, name: TaskC"));
    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "name: TaskA"));
    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "name: TaskB"));
    TEST_ASSERT_GREATER_THAN(200, requestsC);
    TEST_ASSERT_EQUAL(0, FakeTargetPort::errorsLogged);
}

/// @brief Shutdown prints a message and then blocks forever
void test_Shutdown_BlocksForever(void)
{
    TaskMonitor monitor;
    monitor.Shutdown();

    StdoutCapture capture;
    capture.Start();
    Drain();
    const std::string printed = capture.Stop();

    TEST_ASSERT_EQUAL(1, StdoutCapture::Count(printed, "Task Monitor Shutdown"));
    TEST_ASSERT_EQUAL(1, FakeTargetPort::delays.size());
    TEST_ASSERT_EQUAL_UINT32(MAX_DELAY, FakeTargetPort::delays[0]);
}

/// @brief A task that doesn't respond within its timeout is logged once, and the watchdog is no longer kicked, so it
///        resets the device. The monitor keeps processing messages. Must be the last test (see file description).
void test_Violation_LogsOnceAndStopsKickingWatchdog(void)
{
    Drain();
    RegisterAs(&taskD, 100, &RequestD);

    RunForMs(110);                      // request
    TEST_ASSERT_EQUAL(1, requestsD);

    RunForMs(100);                      // waited exactly the timeout: not a violation yet
    TEST_ASSERT_EQUAL(0, FakeTargetPort::errorsLogged);

    const int kicks = FakeTargetPort::watchdogResets;
    RunForMs(EVAL_PERIOD);              // late: violation
    TEST_ASSERT_EQUAL(1, FakeTargetPort::errorsLogged);
    TEST_ASSERT_EQUAL_STRING("TaskMonitor", FakeTargetPort::lastErrorTag);
    TEST_ASSERT_EQUAL_STRING("Task: TaskD - Exceeded Checkin Time. Elapsed time 110 ms, Timeout 100 ms",
                             FakeTargetPort::lastError.c_str());
    TEST_ASSERT_EQUAL(kicks, FakeTargetPort::watchdogResets);

    RunForMs(500);                      // logged only once, watchdog never kicked again
    TEST_ASSERT_EQUAL(1, FakeTargetPort::errorsLogged);
    TEST_ASSERT_EQUAL(kicks, FakeTargetPort::watchdogResets);

    CheckinAs(&taskB);                  // messages are still processed, so senders' queues don't fill
    TEST_ASSERT_EQUAL(0, FakeTargetPort::QueueCount(monitorQ));
}

/// @brief Run all tests (the order matters, see the file description)
/// @return number of failed tests (0 if all passed)
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Initialize_StartsAndKicksWatchdog);
    RUN_TEST(test_Run_EvaluatesEvery10ms);
    RUN_TEST(test_Register_SendsCallingTask);
    RUN_TEST(test_TaskCheckin_SendsCallingTask);
    RUN_TEST(test_CheckinRequest_AfterTimeoutAndOnce);
    RUN_TEST(test_UnsolicitedCheckins_PreventRequests);
    RUN_TEST(test_Register_AgainResetsTask);
    RUN_TEST(test_UnknownMessage_Reported);
    RUN_TEST(test_MaxResponseTimes_PrintedEvery30s);
    RUN_TEST(test_Shutdown_BlocksForever);
    RUN_TEST(test_Violation_LogsOnceAndStopsKickingWatchdog);
    return UNITY_END();
}
