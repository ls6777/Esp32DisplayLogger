/// @file PrintUtils.cpp
/// @brief Target independent stdout API (see PrintUtils.hpp).

#include "PrintUtils.hpp"
#include "StdOutPort.hpp"

PrintBuffer<PrintUtils::BUFFER_SIZE> PrintUtils::printBuffer;
PrintUtils::MirrorFn PrintUtils::mirrorFn = nullptr;

//------------------------------------------------------------------
// SetMirror
//------------------------------------------------------------------
void PrintUtils::SetMirror(MirrorFn mirror)
{
    mirrorFn = mirror;
}

//------------------------------------------------------------------
// WriteAndMirror
//------------------------------------------------------------------
int PrintUtils::WriteAndMirror(const char* data, int len)
{
    const int written = StdOutPort::Write(data, len);

    // Mirror only what the real stdout accepted, so the copy matches the output exactly
    if ((written > 0) && (nullptr != mirrorFn))
    {
        mirrorFn(data, written);
    }
    return written;
}

//------------------------------------------------------------------
// InitStdOut
//------------------------------------------------------------------
void PrintUtils::InitStdOut()
{
    StdOutPort::Redirect();
}

//------------------------------------------------------------------
// Write
//------------------------------------------------------------------
int PrintUtils::Write(const char* data, int len)
{
    return printBuffer.Write(data, len);
}

//------------------------------------------------------------------
// TransmitData
//------------------------------------------------------------------
int PrintUtils::TransmitData()
{
    return printBuffer.Transmit(WriteAndMirror);
}

//------------------------------------------------------------------
// GetHighWaterMark
//------------------------------------------------------------------
int PrintUtils::GetHighWaterMark()
{
    return printBuffer.GetHighWaterMark();
}
