#pragma once

/// @file PrintBuffer.hpp
/// @brief Target independent circular buffer used to hold printf output until it can be transmitted.
/// @details The buffer is a single producer / single consumer queue. The producer is whatever is writing
///          to stdout (printf) and the consumer is the PrintfTask. No OS or C library specifics are used
///          here so the logic can be unit tested on a host machine.

#include <cstddef>
#include <cstring>

/// @brief Circular buffer that holds printf output
/// @tparam Size - total size of the buffer, in bytes. One byte is always kept free to distinguish a full
///                buffer from an empty one, so the usable capacity is Size - 1.
///
/// @note Only one context may call Write at a time (stdout writes must be serialized, see
///       StdOutPort.hpp), and only one context may call Transmit at a time (the PrintfTask).
template <int Size>
class PrintBuffer
{
    static_assert(Size > 1, "PrintBuffer Size must be greater than 1");

    public:

        /// @brief Function used to output data from the buffer (e.g. StdOutPort::Write)
        /// @param[in] data - data to be output
        /// @param[in] len - length of data to output, in bytes (always > 0)
        /// @return number of bytes actually output, or a negative value on error
        using OutputFn = int (*)(const char* data, int len);

        /// @brief Number of bytes the buffer can hold
        static constexpr int CAPACITY = Size - 1;

        /// @brief Copy data into the buffer
        /// @details If there isn't enough space for all of the data, only what fits is copied and the rest
        ///          is dropped.
        /// @param[in] data - data to be written
        /// @param[in] len - length of data to be written, in bytes
        /// @return number of bytes copied into the buffer (0 if data is nullptr or len <= 0)
        int Write(const char* data, int len);

        /// @brief Output all data currently in the buffer
        /// @details Output may take two calls to the output function when the data wraps around the end of
        ///          the buffer. Stops early if the output function outputs less than requested. Data that
        ///          isn't output stays in the buffer for the next call.
        /// @param[in] output - function used to output the data
        /// @return number of bytes output, or the negative error value returned by the output function
        int Transmit(OutputFn output);

        /// @brief Get the number of bytes currently held in the buffer
        /// @return bytes in use
        int GetOccupiedSize() const { return OccupiedSize(head, tail); }

        /// @brief Get the most bytes ever held in the buffer
        /// @details Useful for choosing an appropriate buffer size.
        /// @return high water mark, in bytes
        int GetHighWaterMark() const { return highWaterMark; }

    private:

        /// @brief Calculate the number of bytes held in the buffer
        /// @param[in] cHead - head index to use
        /// @param[in] cTail - tail index to use
        /// @return bytes in use
        static int OccupiedSize(int cHead, int cTail)
        {
            return (cTail >= cHead) ? (cTail - cHead) : (Size - (cHead - cTail));
        }

        char buffer[Size] = {};     ///< Storage for buffered printf data
        int head = 0;               ///< Index of the next byte to output (only modified by Transmit)
        int tail = 0;               ///< Index of the next free byte to write (only modified by Write)
        int highWaterMark = 0;      ///< Most bytes ever held in the buffer
};

//------------------------------------------------------------------
// Write
//------------------------------------------------------------------
template <int Size>
int PrintBuffer<Size>::Write(const char* data, int len)
{
    if ((data == nullptr) || (len <= 0))
    {
        return 0;
    }

    const int cHead = head;
    const int cTail = tail;

    // only write up to the available space, the rest is dropped
    const int spaceRemaining = CAPACITY - OccupiedSize(cHead, cTail);
    if (len > spaceRemaining)
    {
        len = spaceRemaining;
    }

    // copy in up to two pieces: up to the end of the buffer, then any remainder at the start
    const int firstLen = (len < Size - cTail) ? len : (Size - cTail);
    std::memcpy(&buffer[cTail], data, static_cast<std::size_t>(firstLen));
    std::memcpy(&buffer[0], data + firstLen, static_cast<std::size_t>(len - firstLen));

    tail = (cTail + len) % Size;

    // track the high water mark so the developer can determine an appropriate buffer size
    const int occupied = OccupiedSize(cHead, tail);
    if (occupied > highWaterMark)
    {
        highWaterMark = occupied;
    }

    return len;
}

//------------------------------------------------------------------
// Transmit
//------------------------------------------------------------------
template <int Size>
int PrintBuffer<Size>::Transmit(OutputFn output)
{
    int total = 0;

    // at most two passes: data up to the end of the buffer, then data that wrapped to the start
    for (int pass = 0; pass < 2; pass++)
    {
        const int cHead = head;
        const int cTail = tail;
        const int contiguousLen = (cTail >= cHead) ? (cTail - cHead) : (Size - cHead);

        if (contiguousLen == 0)
        {
            break;
        }

        int written = output(&buffer[cHead], contiguousLen);
        if (written < 0)
        {
            return written;
        }
        if (written > contiguousLen)
        {
            written = contiguousLen;
        }

        head = (cHead + written) % Size;
        total += written;

        if (written < contiguousLen)
        {
            break;
        }
    }

    return total;
}
