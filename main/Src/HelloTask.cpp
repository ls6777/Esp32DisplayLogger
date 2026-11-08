/// @file HelloTask.cpp
/// @brief Hello world task implementation (see HelloTask.hpp).

#include "HelloTask.hpp"
#include "PrintUtils.hpp"
#include "TaskMonitor.hpp"
#include "Utils.hpp"

#include <cstdio>

namespace
{
    constexpr uint32_t MSG_Q_LENGTH = 10;                   ///< Number of messages the queue can hold
    constexpr uint32_t HELLO_PERIOD = 1 * MS_PER_SEC;       ///< Period (ms) for printing the hello message
    constexpr uint32_t CHECKIN_TIMEOUT = 5 * MS_PER_SEC;    ///< Task monitor timeout (ms)
    constexpr uint32_t STATS_EVERY = 10;                    ///< Print the high water mark every this many hellos
}

QHandle HelloTask::msgQHandle = nullptr;

//------------------------------------------------------------------
// SendMessage
//------------------------------------------------------------------
void HelloTask::SendMessage(Message::MsgId id)
{
    const Message msg(id);
    const StatusType status = TargetPort::QSend(msgQHandle, &msg);
    ASSERT(PASS == status);
    (void)status;
}

//------------------------------------------------------------------
// TaskCheckin
//------------------------------------------------------------------
void HelloTask::TaskCheckin()
{
    SendMessage(Message::TASK_CHECKIN);
}

//------------------------------------------------------------------
// Shutdown
//------------------------------------------------------------------
void HelloTask::Shutdown()
{
    SendMessage(Message::SHUTDOWN);
}

//------------------------------------------------------------------
// Initialize
//------------------------------------------------------------------
void HelloTask::Initialize()
{
    msgQHandle = TargetPort::CreateQ(MSG_Q_LENGTH, sizeof(Message));
    ASSERT(nullptr != msgQHandle);
    SendMessage(Message::INITIALIZE);
}

//------------------------------------------------------------------
// Run
//------------------------------------------------------------------
void HelloTask::Run()
{
    StatusType status = FALSE;
    Message msg;

    while (true)
    {
        // Wait for new message up to HELLO_PERIOD
        status = TargetPort::QRecv(msgQHandle, &msg, MS_TO_TICKS(HELLO_PERIOD));

        if (TRUE == status)
        {
            switch (msg.msgId)
            {
                case Message::TASK_CHECKIN:
                    TaskMonitor::TaskCheckin();
                    break;

                case Message::INITIALIZE:
                    HandleInitialize();
                    break;

                case Message::SHUTDOWN:
                    HandleShutdown();
                    break;

                default:
                    printf("HelloTask - unknown msgId: %d\r\n", msg.msgId);
                    break;
            }
        }
        else
        {
            SayHello();
        }
    }
}

//------------------------------------------------------------------
// HandleInitialize
//------------------------------------------------------------------
void HelloTask::HandleInitialize()
{
    // Register this task with the Task Monitor
    TaskMonitor::Register(CHECKIN_TIMEOUT, &TaskCheckin);
    printf("HelloTask Initialized\r\n");
}

//------------------------------------------------------------------
// SayHello
//------------------------------------------------------------------
void HelloTask::SayHello()
{
    static uint32_t count = 0;

    count++;
    printf("Hello world! %lu\r\n", (unsigned long)count);

    if ((count % STATS_EVERY) == 0)
    {
        printf("Printf buffer high water mark: %d of %d bytes\r\n", PrintUtils::GetHighWaterMark(),
               PrintUtils::BUFFER_SIZE - 1);
    }
}

//------------------------------------------------------------------
// HandleShutdown
//------------------------------------------------------------------
void HelloTask::HandleShutdown()
{
    printf("HelloTask Shutdown\r\n");
    // wait forever
    DELAY(MAX_DELAY);
}
