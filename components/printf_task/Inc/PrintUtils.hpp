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

        /// @brief Function that receives a copy of everything sent to the real stdout device (see SetMirror())
        /// @param[in] data - data that was output
        /// @param[in] len - length of data, in bytes (always > 0)
        using MirrorFn = void (*)(const char* data, int len);

        PrintUtils() = delete;

        /// @brief Set a function to receive a copy of the output, e.g. to show it on a display
        /// @details The mirror is called from TransmitData() (so from the PrintfTask), with exactly the data the real
        ///          stdout device accepted, in order. It must be quick and must not print. Set it before the
        ///          PrintfTask starts.
        /// @param[in] mirror - function to call, or nullptr for none
        static void SetMirror(MirrorFn mirror);

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
        /// @details Also passes the transmitted data to the mirror, if one is set (see SetMirror()).
        /// @return amount of data transmitted, in bytes, or a negative value on error
        static int TransmitData();

    private:

        /// @brief Output function used by TransmitData(): writes to the real stdout, then to the mirror
        /// @param[in] data - data to output
        /// @param[in] len - length of data, in bytes
        /// @return number of bytes output, or a negative value on error
        static int WriteAndMirror(const char* data, int len);

        static PrintBuffer<BUFFER_SIZE> printBuffer;    ///< The printf buffer
        static MirrorFn mirrorFn;                       ///< Receives a copy of the output (nullptr for none)
};
