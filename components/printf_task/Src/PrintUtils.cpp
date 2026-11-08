/// @file PrintUtils.cpp
/// @brief Target independent stdout API (see PrintUtils.hpp).

#include "PrintUtils.hpp"
#include "StdOutPort.hpp"

PrintBuffer<PrintUtils::BUFFER_SIZE> PrintUtils::printBuffer;

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
    return printBuffer.Transmit(StdOutPort::Write);
}

//------------------------------------------------------------------
// GetHighWaterMark
//------------------------------------------------------------------
int PrintUtils::GetHighWaterMark()
{
    return printBuffer.GetHighWaterMark();
}
