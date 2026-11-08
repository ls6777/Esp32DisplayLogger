#pragma once

/// @file PrintfTask.hpp
/// @brief Task that outputs buffered printf data.

#include "TargetPort.hpp"

/// @brief Printf task
/// @details This class is responsible for sending all buffered printf data to the real stdout device.
///
/// @note This should be one of the lowest priority tasks.
class PrintfTask
{
    public:

        /// @brief Configure stdout rerouting into the printf buffer
        /// @note Must be called before Run(). Call it before creating the tasks, so all task output is buffered.
        static void Initialize();

        /// @brief Execution of the printf task
        /// @details Main loop that periodically outputs all data buffered from printf. Never returns.
        void Run();
};
