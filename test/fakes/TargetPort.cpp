/// @file TargetPort.cpp
/// @brief Host (PC) implementation of the OS port layer, for unit tests (see fakes/TargetPort.hpp).

#include "TargetPort.hpp"

#include <cstdarg>
#include <cstdio>
#include <deque>
#include <memory>

/// @brief In-memory queue
struct FakeQueue
{
    uint32_t length;                        ///< Maximum number of items
    uint32_t itemSize;                      ///< Size of each item, in bytes
    std::deque<std::vector<uint8_t>> items; ///< Items waiting, oldest first
};

namespace
{
    /// @brief Every queue created since the last Reset()
    std::vector<std::unique_ptr<FakeQueue>> queues;
}

uint32_t FakeTargetPort::timeMs = 0;
uint32_t FakeTargetPort::cpuBusyPercent = 0;
uint32_t FakeTargetPort::cpuIdleTime = 0;
uint32_t FakeTargetPort::cpuTotalTime = 0;
TaskHandle FakeTargetPort::currentTask = nullptr;
int FakeTargetPort::recvBudget = -1;
int FakeTargetPort::delayBudget = -1;
std::function<void()> FakeTargetPort::onDelay;
std::function<void()> FakeTargetPort::onRecv;
std::vector<uint32_t> FakeTargetPort::delays;
std::vector<FakeTargetPort::SentItem> FakeTargetPort::sent;
int FakeTargetPort::watchdogStarts = 0;
int FakeTargetPort::watchdogResets = 0;
int FakeTargetPort::errorsLogged = 0;
const char* FakeTargetPort::lastErrorTag = nullptr;
std::string FakeTargetPort::lastError;

//------------------------------------------------------------------
// FakeTargetPort
//------------------------------------------------------------------
void FakeTargetPort::Reset()
{
    queues.clear();
    currentTask = nullptr;
    cpuBusyPercent = 0;
    recvBudget = -1;
    delayBudget = -1;
    onDelay = nullptr;
    onRecv = nullptr;
    delays.clear();
    sent.clear();
    watchdogStarts = 0;
    watchdogResets = 0;
    errorsLogged = 0;
    lastErrorTag = nullptr;
    lastError.clear();
}

void FakeTargetPort::RecordError(const char* tag, const char* format, ...)
{
    errorsLogged++;
    lastErrorTag = tag;

    char buf[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    lastError = buf;
}

void FakeTargetPort::AdvanceTime(uint32_t ms)
{
    timeMs += ms;

    const uint32_t total = ms * 1000 * FAKE_CORES;
    cpuTotalTime += total;
    cpuIdleTime += total - ((total / 100) * cpuBusyPercent);
}

void FakeTargetPort::Delay(uint32_t ticks)
{
    if (onDelay)
    {
        onDelay();
    }

    delays.push_back(ticks);

    if ((ticks == MAX_DELAY) || (delayBudget == 0))
    {
        throw StopRun();
    }
    if (delayBudget > 0)
    {
        delayBudget--;
    }

    AdvanceTime(ticks);
}

size_t FakeTargetPort::QueueCount(QHandle q)
{
    return q->items.size();
}

std::vector<FakeTargetPort::SentItem> FakeTargetPort::SentToOtherQueues(QHandle q)
{
    std::vector<SentItem> result;
    for (const SentItem& item : sent)
    {
        if (item.queue != q)
        {
            result.push_back(item);
        }
    }
    return result;
}

//------------------------------------------------------------------
// TargetPort
//------------------------------------------------------------------
TaskHandle TargetPort::GetCurrTaskHandle()
{
    return FakeTargetPort::currentTask;
}

const char* TargetPort::GetTaskName(TaskHandle taskId)
{
    return (taskId != nullptr) ? taskId->name : "(null)";
}

uint32_t TargetPort::GetTimeMs()
{
    return FakeTargetPort::timeMs;
}

void TargetPort::StartWatchdog()
{
    FakeTargetPort::watchdogStarts++;
}

void TargetPort::WatchdogReset()
{
    FakeTargetPort::watchdogResets++;
}

QHandle TargetPort::CreateQ(uint32_t qLength, uint32_t itemSize)
{
    queues.push_back(std::make_unique<FakeQueue>(FakeQueue{qLength, itemSize, {}}));
    return queues.back().get();
}

StatusType TargetPort::QSend(QHandle qId, const void* buf)
{
    assert(nullptr != qId);
    if (qId->items.size() >= qId->length)
    {
        return FAIL;
    }

    const auto* bytes = static_cast<const uint8_t*>(buf);
    std::vector<uint8_t> item(bytes, bytes + qId->itemSize);
    qId->items.push_back(item);
    FakeTargetPort::sent.push_back(FakeTargetPort::SentItem{qId, item});
    return PASS;
}

StatusType TargetPort::QRecv(QHandle qId, void* buf, uint32_t timeout)
{
    assert(nullptr != qId);

    if (FakeTargetPort::recvBudget == 0)
    {
        throw StopRun();
    }
    if (FakeTargetPort::recvBudget > 0)
    {
        FakeTargetPort::recvBudget--;
    }

    if (FakeTargetPort::onRecv)
    {
        FakeTargetPort::onRecv();
    }

    if (qId->items.empty())
    {
        if (timeout == MAX_DELAY)
        {
            throw StopRun();
        }
        FakeTargetPort::AdvanceTime(timeout);
        return FAIL;
    }

    std::memcpy(buf, qId->items.front().data(), qId->itemSize);
    qId->items.pop_front();
    return PASS;
}

void TargetPort::GetTaskList(char* buf, size_t size)
{
    std::snprintf(buf, size, "fake task list\r\n");
}

void TargetPort::GetCpuTimes(uint32_t& idleTime, uint32_t& totalTime)
{
    idleTime = FakeTargetPort::cpuIdleTime;
    totalTime = FakeTargetPort::cpuTotalTime;
}

void TargetPort::GetRunTimeStats(char* buf, size_t size)
{
    std::snprintf(buf, size, "fake run time stats\r\n");
}
