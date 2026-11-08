#pragma once

/// @file PrintUtils.hpp
/// @brief Target independent stdout API used by the PrintfTask.
/// @details Owns the printf buffer (PrintBuffer.hpp) and ties it to the target's stdout (StdOutPort.hpp).

#include "PrintBuffer.hpp"

/// @brief stdout functions used by the PrintfTask
/// @details Static class: there is only one stdout, so there is only one printf buffer. All members are
///          static, so it can be reached from the C library's stdout callback, and it can't be instantiated.
class PrintUtils
{
    public:

        /// @brief Total size of the printf buffer, in bytes (usable capacity is one byte less)
        /// @details Use GetHighWaterMark() to choose an appropriate size.
        static constexpr int BUFFER_SIZE = 15000;

        PrintUtils() = delete;

        /// @brief Set up the reroute of stdout
        /// @details Redirects stdout, and therefore printf, into the printf buffer. Must be called once
        ///          before any output is expected to be buffered.
        static void InitStdOut();

        /// @brief Copy data into the printf buffer
        /// @details Called by the redirected stdout write (see StdOutPort.hpp). If there isn't enough space
        ///          for all of the data, only what fits is copied and the rest is dropped.
        /// @param[in] data - data to be written
        /// @param[in] len - length of data to be written, in bytes
        /// @return number of bytes copied into the buffer
        static int Write(const char* data, int len);

        /// @brief Get the high water mark of the printf buffer
        /// @return most space taken in the printf buffer since application start, in bytes
        static int GetHighWaterMark();

        /// @brief Transmit all data available in the buffer to the real stdout device
        /// @return amount of data transmitted, in bytes, or a negative value on error
        static int TransmitData();

    private:

        static PrintBuffer<BUFFER_SIZE> printBuffer;    ///< The printf buffer
};
