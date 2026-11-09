#pragma once

/// @file CpuUsage.hpp
/// @brief Target independent CPU usage calculation.
/// @details Turns two readings of the idle and total time counters (TargetPort::GetCpuTimes()) into the percentage of
///          CPU time used between them. No OS or target specifics, so it can be unit tested on a host machine.

#include <cstdint>

/// @brief Calculates CPU usage from successive idle and total time counter readings
/// @details Usage over an interval is 100% minus the share of the interval spent in the idle task. The counters wrap
///          around at 2^32 and only their differences are used, so intervals of up to 2^32 counter units work across
///          the wrap.
class CpuUsage
{
    public:

        /// @brief Add a new counter reading and calculate the usage since the previous one
        /// @details The first reading only sets the starting point. If the total time didn't advance, the previous
        ///          usage is kept.
        /// @param[in] idleTime - idle time counter (see TargetPort::GetCpuTimes())
        /// @param[in] totalTime - total time counter (see TargetPort::GetCpuTimes())
        /// @return CPU usage since the previous reading, 0 to 100 %, rounded to the nearest percent
        uint32_t Update(uint32_t idleTime, uint32_t totalTime)
        {
            if (hasReading)
            {
                const uint32_t idleDelta = idleTime - prevIdle;
                const uint32_t totalDelta = totalTime - prevTotal;

                if (totalDelta > 0)
                {
                    // The idle counter can lag the total slightly, so idle may appear to exceed the interval
                    const uint32_t busy = (idleDelta < totalDelta) ? (totalDelta - idleDelta) : 0;
                    percent = static_cast<uint32_t>(((static_cast<uint64_t>(busy) * 100) + (totalDelta / 2)) /
                                                    totalDelta);
                    hasValue = true;
                }
            }

            prevIdle = idleTime;
            prevTotal = totalTime;
            hasReading = true;
            return percent;
        }

        /// @brief Check whether a usage has been calculated yet (needs two readings with time between them)
        /// @return true once GetPercent() holds a measured value
        bool HasValue() const
        {
            return hasValue;
        }

        /// @brief Get the last calculated usage
        /// @return CPU usage, 0 to 100 % (0 until HasValue())
        uint32_t GetPercent() const
        {
            return percent;
        }

    private:

        uint32_t prevIdle = 0;      ///< Idle counter at the previous reading
        uint32_t prevTotal = 0;     ///< Total counter at the previous reading
        uint32_t percent = 0;       ///< Last calculated usage, in %
        bool hasReading = false;    ///< True once the first reading has been taken
        bool hasValue = false;      ///< True once a usage has been calculated
};
