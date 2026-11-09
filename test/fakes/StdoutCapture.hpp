#pragma once

/// @file StdoutCapture.hpp
/// @brief Captures everything written to stdout (e.g. by printf in the code under test), for unit tests.

#include <cstdio>
#include <string>

#ifdef _WIN32
#include <io.h>
#define CAPTURE_DUP _dup
#define CAPTURE_DUP2 _dup2
#define CAPTURE_CLOSE _close
#define CAPTURE_FILENO _fileno
#else
#include <unistd.h>
#define CAPTURE_DUP dup
#define CAPTURE_DUP2 dup2
#define CAPTURE_CLOSE close
#define CAPTURE_FILENO fileno
#endif

/// @brief Redirects stdout to a temporary file between Start() and Stop()
/// @note Always call Stop() before any Unity assertion: a failing assertion returns from the test immediately, and
///       Unity prints its results to stdout.
class StdoutCapture
{
    public:

        /// @brief Start capturing stdout
        void Start()
        {
            std::fflush(stdout);
            file = std::fopen(FILE_NAME, "w+");
            savedFd = CAPTURE_DUP(CAPTURE_FILENO(stdout));
            CAPTURE_DUP2(CAPTURE_FILENO(file), CAPTURE_FILENO(stdout));
        }

        /// @brief Stop capturing and restore stdout
        /// @return everything written to stdout since Start()
        std::string Stop()
        {
            std::fflush(stdout);
            CAPTURE_DUP2(savedFd, CAPTURE_FILENO(stdout));
            CAPTURE_CLOSE(savedFd);

            std::string text;
            std::rewind(file);
            char buf[256];
            size_t len = 0;
            while ((len = std::fread(buf, 1, sizeof(buf), file)) > 0)
            {
                text.append(buf, len);
            }
            std::fclose(file);
            std::remove(FILE_NAME);
            return text;
        }

        /// @brief Count the occurrences of a piece of text
        /// @param[in] text - text to search
        /// @param[in] what - text to count
        /// @return number of non-overlapping occurrences
        static int Count(const std::string& text, const std::string& what)
        {
            int count = 0;
            for (size_t pos = text.find(what); pos != std::string::npos; pos = text.find(what, pos + what.size()))
            {
                count++;
            }
            return count;
        }

    private:

        static constexpr const char* FILE_NAME = "stdout_capture.tmp";  ///< Temporary file, in the working directory

        std::FILE* file = nullptr;  ///< File stdout is redirected to
        int savedFd = -1;           ///< Copy of the original stdout file descriptor
};
