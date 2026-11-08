#pragma once

/// @file StdOutPort.hpp
/// @brief Target specific stdout hooks used by PrintUtils.
/// @details These are the only functions that need to be implemented when porting PrintfTask to a new
///          target / C library. See StdOutPort.cpp for the ESP32 (Picolibc and Newlib) implementation.

/// @brief Target specific stdout hooks
/// @details Static class: there is only one stdout. Any state the implementation needs (such as the saved
///          C library write function) is kept private in StdOutPort.cpp, since it depends on the target.
class StdOutPort
{
    public:

        StdOutPort() = delete;

        /// @brief Redirect stdout so data written by printf goes into the printf buffer
        /// @details The implementation must:
        ///          - save whatever is needed for Write to output to the real stdout device
        ///          - route all stdout writes to PrintUtils::Write (see PrintUtils.hpp)
        ///
        ///          If the target/C library doesn't serialize stdout access between tasks, the redirected
        ///          write must be protected with a lock/mutex.
        static void Redirect();

        /// @brief Write data to the real stdout device (e.g. UART)
        /// @param[in] data - data to output
        /// @param[in] len - length of data to output, in bytes
        /// @return number of bytes output, or a negative value on error
        static int Write(const char* data, int len);
};
