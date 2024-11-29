#include "OutputDevice.hpp"
#include <list>


void OutputDevice::callback([[maybe_unused]] MIDIMessage msg, [[maybe_unused]]uintptr_t dw1, [[maybe_unused]]uintptr_t dw2)
{
};

#ifdef _WIN32
#include "OutputDevice.win32.cpp"
#else
#include "OutputDevice.unix.cpp"
#endif
