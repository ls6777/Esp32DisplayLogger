#pragma once

/// @file HelloTask.hpp
/// @brief Hello world task: prints through the PrintfTask and checks in with the TaskMonitor.

#include "Message.hpp"
#include "TargetPort.hpp"

/// @brief Hello world task
/// @details Shows "Hello World!" on the LCD once, at start up. Then prints a hello message once a second. The output
///          goes into the printf buffer and is sent to the UART by the PrintfTask. Every 10 messages it also prints
///          the printf buffer high water mark.
///
///          Registers with the task monitor with a 5 second timeout and checks in when the monitor asks.
class HelloTask
{
    public:

        /// @brief Create this task's message queue and post the initialize message to it
        /// @note Must be called before Run().
        void Initialize();

        /// @brief Execution of HelloTask
        /// @details Main loop that waits up to 1 second for messages posted to the queue and processes them. Prints
        ///          the hello message whenever the wait times out. Never returns.
        void Run();

        /// @brief Request a checkin from this task (task monitor checkin callback)
        /// @details Posts a checkin message to this task's queue. The task then checks in with the task monitor.
        static void TaskCheckin();

        /// @brief Post a message to shut down this task
        void Shutdown();

    private:

        /// @brief Post a message to this task's queue
        /// @param[in] id - ID of the message to send
        static void SendMessage(Message::MsgId id);

        /// @brief Handle the initialize message: register with the task monitor and show "Hello World!" on the LCD
        void HandleInitialize();

        /// @brief Print the hello message, and the printf buffer high water mark every 10 messages
        void SayHello();

        /// @brief Handle the shutdown message: print a message and stop processing
        void HandleShutdown();

        static QHandle msgQHandle;  ///< This task's message queue
};
