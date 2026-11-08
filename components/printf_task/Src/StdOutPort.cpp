/// @file StdOutPort.cpp
/// @brief StdOutPort implementation for ESP32 / ESP-IDF, supporting both Picolibc and Newlib.
/// @details ESP-IDF v6.0 and later use Picolibc by default (CONFIG_LIBC_PICOLIBC). Earlier versions, or
///          projects that select CONFIG_LIBC_NEWLIB, use Newlib. In both cases the C library's stdout write
///          function is saved and replaced with one that writes into the printf buffer. The saved function
///          is then used to output the buffered data when the PrintfTask runs.
///
///          This is the only file that needs to change when porting to another target. For example, on an
///          STM32 using STM32CubeIDE (newlib-nano), Redirect can be empty and the _write syscall overridden
///          instead. _write is called by the C library, so it must have C linkage:
/// @code
/// extern "C" int _write(int fd, char* ptr, int len)
/// {
///     (void)fd;
///     return PrintUtils::Write(ptr, len);
/// }
///
/// void StdOutPort::Redirect()
/// {
/// }
///
/// int StdOutPort::Write(const char* data, int len)
/// {
///     const auto bytes = reinterpret_cast<const uint8_t*>(data);
///     return (HAL_UART_Transmit(&huart2, bytes, len, HAL_MAX_DELAY) == HAL_OK) ? len : -1;
/// }
/// @endcode

#include "StdOutPort.hpp"
#include "PrintUtils.hpp"

#include "sdkconfig.h"

#include <cstdio>

#if CONFIG_LIBC_PICOLIBC

//==================================================================
// Picolibc
//==================================================================
#include <cstdint>
#include <stdio-bufio.h>

namespace
{
    /// @brief Type of Picolibc's buffered file write function
    using WriteFn = ssize_t (*)(int fd, const void* data, size_t len);

    /// @brief Original stdout write function, used to output the buffered data from the PrintfTask
    WriteFn realStdOutWrite = nullptr;

    /// @brief File descriptor of stdout, passed to realStdOutWrite
    int stdOutFd = 1;

    //------------------------------------------------------------------
    // WriteReroute
    //------------------------------------------------------------------
    /// @brief Replacement stdout write function, called by Picolibc for printf output
    /// @details Picolibc holds the stdout lock while calling this, so writes from different tasks are
    ///          already serialized and no extra locking is needed here.
    /// @param[in] fd - file descriptor (unused)
    /// @param[in] data - data to be written
    /// @param[in] len - length of data to be written, in bytes
    /// @return number of bytes written to the printf buffer
    ssize_t WriteReroute(int fd, const void* data, size_t len)
    {
        (void)fd;
        return PrintUtils::Write(static_cast<const char*>(data), static_cast<int>(len));
    }
}

//------------------------------------------------------------------
// Redirect
//------------------------------------------------------------------
void StdOutPort::Redirect()
{
    // ESP-IDF sets stdout up as a Picolibc buffered (bufio) file using a file descriptor
    auto* const bufio = reinterpret_cast<__file_bufio*>(stdout);

    stdOutFd = static_cast<int>(reinterpret_cast<intptr_t>(bufio->ptr));
    realStdOutWrite = bufio->write_int;
    bufio->write_int = WriteReroute;
}

//------------------------------------------------------------------
// Write
//------------------------------------------------------------------
int StdOutPort::Write(const char* data, int len)
{
    return static_cast<int>(realStdOutWrite(stdOutFd, data, static_cast<size_t>(len)));
}

#else

//==================================================================
// Newlib
//==================================================================

namespace
{
    /// @brief Type of Newlib's file write function
    using WriteFn = int (*)(_reent* r, void* cookie, const char* data, int len);

    /// @brief Original stdout write function, used to output the buffered data from the PrintfTask
    WriteFn realStdOutWrite = nullptr;

    //------------------------------------------------------------------
    // WriteReroute
    //------------------------------------------------------------------
    /// @brief Replacement stdout write function, called by Newlib for printf output
    /// @details ESP-IDF's Newlib already serializes access to stdout between tasks, so no extra locking
    ///          is needed here.
    /// @param[in] r - Newlib reentrancy structure (unused)
    /// @param[in] cookie - stream cookie (unused)
    /// @param[in] data - data to be written
    /// @param[in] len - length of data to be written, in bytes
    /// @return number of bytes written to the printf buffer
    int WriteReroute(_reent* r, void* cookie, const char* data, int len)
    {
        (void)r;
        (void)cookie;
        return PrintUtils::Write(data, len);
    }
}

//------------------------------------------------------------------
// Redirect
//------------------------------------------------------------------
void StdOutPort::Redirect()
{
    realStdOutWrite = _GLOBAL_REENT->_stdout->_write;
    _GLOBAL_REENT->_stdout->_write = WriteReroute;
}

//------------------------------------------------------------------
// Write
//------------------------------------------------------------------
int StdOutPort::Write(const char* data, int len)
{
    return realStdOutWrite(_GLOBAL_REENT, _GLOBAL_REENT->_stdout, data, len);
}

#endif
