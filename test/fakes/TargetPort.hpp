#pragma once

/// @file TargetPort.hpp
/// @brief Host (PC) implementation of the OS port layer, for unit tests.
/// @details Replaces components/common/Inc/TargetPort.hpp in the test build. It has the same types, macros and
///          TargetPort functions, implemented with in-memory queues and a fake clock, plus FakeTargetPort to control
///          and inspect them from the tests.
///
///          A task's Run() never returns, so the fake ends it by throwing StopRun when:
///          - QRecv() has been called FakeTargetPort::recvBudget times
///          - QRecv() is asked to wait forever (MAX_DELAY) on an empty queue, which would block forever
///          - DELAY() has been called FakeTargetPort::delayBudget times, or is asked to wait forever
///
///          The fake clock only moves forward when a task waits: QRecv() on an empty queue advances it by the
///          timeout, and DELAY() by the delay. One tick is one millisecond.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#define PASS 1      ///< Status returned when an operation succeeds
#define FAIL 0      ///< Status returned when an operation fails

#define TRUE 1      ///< Boolean true, as used by the OS
#define FALSE 0     ///< Boolean false, as used by the OS

#define MAX_DELAY 0xFFFFFFFFu   ///< Wait forever (when used as a timeout, in OS ticks)

/// @brief Assert that a condition is true
/// @param x - condition that must be true
#define ASSERT(x) assert(x)

/// @brief Log an error message (the fake records it in FakeTargetPort instead of printing it)
/// @param tag - tag identifying the source of the message
/// @param format - printf style format string, followed by its arguments
#define LOG_ERROR(tag, format, ...) FakeTargetPort::RecordError(tag, format, ##__VA_ARGS__)

/// @brief Convert milliseconds to OS ticks (1 tick = 1 ms)
/// @param x - time in milliseconds
#define MS_TO_TICKS(x) (x)

/// @brief Block the calling task for a number of OS ticks
/// @param x - time in OS ticks (see MS_TO_TICKS, MAX_DELAY)
#define DELAY(x) FakeTargetPort::Delay(x)

/// @brief A fake task. Tests create these and set FakeTargetPort::currentTask to choose the calling task.
struct FakeTask
{
    const char* name;   ///< Name returned by TargetPort::GetTaskName()
};

struct FakeQueue;   // defined in TargetPort.cpp

using TaskHandle = FakeTask*;   ///< Handle to a task
using QHandle = FakeQueue*;     ///< Handle to a message queue
using StatusType = int;         ///< Status returned by OS calls (PASS / FAIL)

/// @brief OS functions (same API as the target TargetPort)
class TargetPort
{
    public:

        TargetPort() = delete;

        /// @brief Get the handle of the calling task
        /// @return FakeTargetPort::currentTask
        static TaskHandle GetCurrTaskHandle();

        /// @brief Get the name of a task
        /// @param[in] taskId - task to get the name of
        /// @return name of the task
        static const char* GetTaskName(TaskHandle taskId);

        /// @brief Get the fake time
        /// @return FakeTargetPort::timeMs
        static uint32_t GetTimeMs();

        /// @brief Count a watchdog start (FakeTargetPort::watchdogStarts)
        static void StartWatchdog();

        /// @brief Count a watchdog kick (FakeTargetPort::watchdogResets)
        static void WatchdogReset();

        /// @brief Create an in-memory queue
        /// @param[in] qLength - maximum number of items the queue can hold
        /// @param[in] itemSize - size of each item in the queue, in bytes
        /// @return handle for the queue created
        static QHandle CreateQ(uint32_t qLength, uint32_t itemSize);

        /// @brief Copy an item into a queue, and record it in FakeTargetPort::sent
        /// @param[in] qId - queue to put the message in
        /// @param[in] buf - pointer to the item to copy into the queue
        /// @return PASS if successful, FAIL if the queue is full
        static StatusType QSend(QHandle qId, const void* buf);

        /// @brief Get the oldest item out of a queue
        /// @details If the queue is empty, advances the fake clock by the timeout and returns FAIL. Throws StopRun
        ///          when the budget is used up, or when the queue is empty and the timeout is MAX_DELAY.
        /// @param[in] qId - queue to get the message from
        /// @param[out] buf - pointer to where the item from the queue is copied
        /// @param[in] timeout - max time to wait for a message, in OS ticks
        /// @return PASS if a message was received, FAIL if the timeout expired
        static StatusType QRecv(QHandle qId, void* buf, uint32_t timeout);

        /// @brief Write a fixed placeholder task list
        /// @param[out] buf - buffer to write the null-terminated text to
        /// @param[in] size - size of the buffer, in bytes
        static void GetTaskList(char* buf, size_t size);

        /// @brief Write fixed placeholder run-time statistics
        /// @param[out] buf - buffer to write the null-terminated text to
        /// @param[in] size - size of the buffer, in bytes
        static void GetRunTimeStats(char* buf, size_t size);
};

/// @brief Thrown by the fake to end a task's Run() loop (see the file description)
struct StopRun
{
};

/// @brief Controls and records the fake OS
/// @details Static class: there is only one fake OS.
class FakeTargetPort
{
    public:

        /// @brief An item sent to a queue
        struct SentItem
        {
            QHandle queue;              ///< Queue the item was sent to
            std::vector<uint8_t> data;  ///< Copy of the item

            /// @brief Interpret the item as a message type
            /// @tparam T - message type (must match the queue's item size)
            /// @return copy of the item
            template <typename T>
            T As() const
            {
                assert(data.size() == sizeof(T));
                T item;
                std::memcpy(static_cast<void*>(&item), data.data(), sizeof(T));
                return item;
            }
        };

        FakeTargetPort() = delete;

        /// @brief Delete all queues and clear the records, budgets and hooks
        /// @details The clock is left as it is: the code under test keeps times in static variables, so time must
        ///          never go backwards within one test program.
        static void Reset();

        /// @brief Called by LOG_ERROR: counts the error and keeps its tag and formatted message
        /// @param[in] tag - tag of the error
        /// @param[in] format - printf style format string, followed by its arguments
        static void RecordError(const char* tag, const char* format, ...) __attribute__((format(printf, 2, 3)));

        /// @brief Called by DELAY: records the delay and advances the clock
        /// @details Calls onDelay first. Throws StopRun if the delay is MAX_DELAY or the budget is used up.
        /// @param[in] ticks - delay, in ticks
        static void Delay(uint32_t ticks);

        /// @brief Get the number of items waiting in a queue
        /// @param[in] q - queue to check
        /// @return items in the queue
        static size_t QueueCount(QHandle q);

        /// @brief Get the items sent to every queue except one, oldest first
        /// @param[in] q - queue to leave out
        /// @return items sent to other queues
        static std::vector<SentItem> SentToOtherQueues(QHandle q);

        static uint32_t timeMs;             ///< The fake clock, in ms (= ticks)
        static TaskHandle currentTask;      ///< Task returned by GetCurrTaskHandle()
        static int recvBudget;              ///< QRecv() calls left before StopRun (negative = unlimited)
        static int delayBudget;             ///< DELAY() calls left before StopRun (negative = unlimited)
        static std::function<void()> onDelay;   ///< Called at the start of every DELAY() (e.g. to inject data)
        static std::vector<uint32_t> delays;    ///< Every DELAY() argument, in order
        static std::vector<SentItem> sent;      ///< Every item successfully sent to any queue, in order
        static int watchdogStarts;          ///< Number of StartWatchdog() calls
        static int watchdogResets;          ///< Number of WatchdogReset() calls
        static int errorsLogged;            ///< Number of LOG_ERROR calls
        static const char* lastErrorTag;    ///< Tag of the last LOG_ERROR call
        static std::string lastError;       ///< Formatted message of the last LOG_ERROR call
};
