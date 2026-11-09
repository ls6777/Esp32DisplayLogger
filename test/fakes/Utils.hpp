#pragma once

/// @file Utils.hpp
/// @brief Test build copy of components/common/Inc/Utils.hpp.
/// @details The real Utils.hpp includes "TargetPort.hpp", and a quoted include always looks in the including file's
///          own directory first, so it would pick up the real (FreeRTOS) TargetPort.hpp next to it. This copy lives
///          next to the fake TargetPort.hpp instead. Keep its contents the same as the real file.

#include "TargetPort.hpp"

#include <cstdint>

/// @brief Block the calling task for a number of milliseconds
/// @param x - time in milliseconds
#define DELAY_MS(x) DELAY(MS_TO_TICKS(x))

static constexpr uint32_t MS_PER_SEC = 1000;    ///< Milliseconds per second
